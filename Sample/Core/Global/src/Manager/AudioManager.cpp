#include <Manager/AudioManager.hpp>

#include <Filters/SoundFilter.hpp>
#include <Manager/TimeManager.hpp>
#include <SystemConfigBase.hpp>

#include "AudioEffectLuaRuntime.hpp"

#include <Utf8Path.hpp>

#include <SFML/Audio/PlaybackDevice.hpp>

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr float SpatialMinDistance = 64.0f;

using ludork::global::audio::AudioEffectAttacher;
using ludork::global::audio::ManagedMusic;
using ludork::global::audio::ManagedSound;

void requireLogicThreadAudioLifecycle() {
    if (ludork::global::audio::isManagedAudioCallbackThread()) {
        throw std::logic_error(
            "Audio Manager lifecycle cannot change from an effect processor");
    }
}

struct SoundRecord {
    std::shared_ptr<ManagedSound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> parent;
    float baseVolume = 100.0f;
    float basePitch = 1.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct VoiceRecord {
    std::shared_ptr<ManagedSound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> refActor;
    float baseVolume = 100.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct MusicRecord {
    std::shared_ptr<ManagedMusic> music;
    std::string filePath;
    float baseVolume = 100.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

enum class SoundCategory {
    Unmanaged,
    Sound,
    Voice
};

std::unordered_map<std::string, std::shared_ptr<sf::SoundBuffer>> soundBuffers;
std::unordered_map<std::string, std::size_t> soundBufferCounts;
std::vector<SoundRecord> sounds;
VoiceRecord voice;
std::unordered_map<std::string, MusicRecord> musics;
AudioEffectAttacher soundEffect;
AudioEffectAttacher voiceEffect;
AudioEffectAttacher musicEffect;
std::uint64_t soundGeneration = 0;
std::uint64_t voiceGeneration = 0;
std::unordered_map<std::string, std::uint64_t> musicGenerations;
bool shuttingDown = false;
std::recursive_mutex audioMutex;
std::condition_variable_any audioCreationCondition;
std::size_t audioCreationsInFlight = 0;

class AudioCreationScope {
public:
    AudioCreationScope() = default;
    AudioCreationScope(const AudioCreationScope&) = delete;
    AudioCreationScope& operator=(const AudioCreationScope&) = delete;

    ~AudioCreationScope() {
        if (!active_) {
            return;
        }
        {
            const std::lock_guard<std::recursive_mutex> lock(audioMutex);
            --audioCreationsInFlight;
        }
        audioCreationCondition.notify_all();
    }

    void activate() noexcept {
        ++audioCreationsInFlight;
        active_ = true;
    }

private:
    bool active_ = false;
};

sf::Vector3f positionOf(
    const std::shared_ptr<sf::Transformable>& transformable) {
    if (transformable == nullptr) {
        return {};
    }
    const sf::Vector2f position = transformable->getPosition();
    return {position.x, position.y, 0.0f};
}

void retainBuffer(const std::string& filePath,
                  const std::shared_ptr<sf::SoundBuffer>& buffer) {
    soundBuffers[filePath] = buffer;
    ++soundBufferCounts[filePath];
}

std::shared_ptr<sf::SoundBuffer> releaseBuffer(const std::string& filePath) {
    const auto iterator = soundBufferCounts.find(filePath);
    if (iterator == soundBufferCounts.end()) {
        return nullptr;
    }
    if (iterator->second > 1) {
        --iterator->second;
        return nullptr;
    }
    soundBufferCounts.erase(iterator);
    const auto bufferIterator = soundBuffers.find(filePath);
    if (bufferIterator == soundBuffers.end()) {
        return nullptr;
    }
    std::shared_ptr<sf::SoundBuffer> buffer = std::move(bufferIterator->second);
    soundBuffers.erase(bufferIterator);
    return buffer;
}

std::string caseAlias(const std::string& value) {
    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    if (upper != value) {
        return upper;
    }
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return lower;
}

SoundRecord* findSoundRecord(const sf::Sound* sound) {
    const auto iterator = std::find_if(sounds.begin(), sounds.end(),
                                       [sound](const SoundRecord& record) {
                                           return record.sound.get() == sound;
                                       });
    return iterator == sounds.end() ? nullptr : &*iterator;
}

MusicRecord* findMusicRecord(const sf::Music* music) {
    for (auto& [type, record] : musics) {
        static_cast<void>(type);
        if (record.music.get() == music) {
            return &record;
        }
    }
    return nullptr;
}

template <typename Source>
std::shared_ptr<AudioEffectControl> attachEffect(
    Source& source, const AudioEffectAttacher& effect) {
    if (!effect) {
        return nullptr;
    }
    const std::optional<std::uint32_t> sampleRate =
        sf::PlaybackDevice::getDeviceSampleRate();
    if (!sampleRate.has_value() || *sampleRate == 0) {
        throw std::runtime_error(
            "Audio playback device sample rate is unavailable");
    }
    auto control = std::make_shared<AudioEffectControl>();
    source.beginEffectAttachment(control);
    try {
        effect(source, control, *sampleRate);
        source.finishEffectAttachment();
    } catch (...) {
        source.abortEffectAttachment();
        throw;
    }
    return control;
}

template <typename Source>
bool isFinished(const Source& source) noexcept {
    return source.getStatus() == sf::SoundSource::Status::Stopped &&
           source.isNaturalInputDrained();
}

void applyAudioFilter(sf::SoundSource& source, const SoundFilter& filter) {
    if (filter.pan.has_value()) {
        source.setPan(*filter.pan);
    }
    if (filter.spatial.has_value()) {
        source.setSpatializationEnabled(*filter.spatial);
    }
    if (filter.position.has_value()) {
        source.setPosition(*filter.position);
    }
    if (filter.direction.has_value()) {
        source.setDirection(*filter.direction);
    }
    if (filter.cone.has_value()) {
        source.setCone(*filter.cone);
    }
    if (filter.velocity.has_value()) {
        source.setVelocity(*filter.velocity);
    }
    if (filter.dopplerFactor.has_value()) {
        source.setDopplerFactor(*filter.dopplerFactor);
    }
    if (filter.directionalAttenuationFactor.has_value()) {
        source.setDirectionalAttenuationFactor(
            *filter.directionalAttenuationFactor);
    }
    if (filter.relativeToListener.has_value()) {
        source.setRelativeToListener(*filter.relativeToListener);
    }
    if (filter.minDistance.has_value()) {
        source.setMinDistance(*filter.minDistance);
    } else if (filter.spatial.value_or(false)) {
        source.setMinDistance(SpatialMinDistance);
    }
    if (filter.maxDistance.has_value()) {
        source.setMaxDistance(*filter.maxDistance);
    }
    if (filter.minGain.has_value()) {
        source.setMinGain(*filter.minGain);
    }
    if (filter.maxGain.has_value()) {
        source.setMaxGain(*filter.maxGain);
    }
    if (filter.attenuation.has_value()) {
        source.setAttenuation(*filter.attenuation);
    }
}

void applySoundSettings(sf::Sound& sound, const SoundFilter& filter) {
    if (filter.loop.has_value()) {
        sound.setLooping(*filter.loop);
    }
    if (filter.offset.has_value()) {
        sound.setPlayingOffset(*filter.offset);
    }
    applyAudioFilter(sound, filter);
}

void applyMusicSettings(sf::Music& music, const MusicFilter& filter) {
    if (filter.loop.has_value()) {
        music.setLooping(*filter.loop);
    }
    if (filter.offset.has_value()) {
        music.setPlayingOffset(*filter.offset);
    }
    if (filter.loopPoint.has_value()) {
        music.setLoopPoints(*filter.loopPoint);
    }
    applyAudioFilter(music, filter);
    music.setSpatializationEnabled(false);
}

bool isSpatial(const SoundFilter* filter) {
    return filter != nullptr && filter->spatial.value_or(false);
}

void stopSoundRecords(std::vector<SoundRecord>& records) {
    std::vector<std::string> filePaths;
    filePaths.reserve(records.size());
    for (SoundRecord& record : records) {
        if (record.sound != nullptr) {
            record.sound->stop();
            record.sound.reset();
        }
        filePaths.push_back(std::move(record.filePath));
    }
    records.clear();
    std::vector<std::shared_ptr<sf::SoundBuffer>> releasedBuffers;
    releasedBuffers.reserve(filePaths.size());
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        for (const std::string& filePath : filePaths) {
            std::shared_ptr<sf::SoundBuffer> buffer = releaseBuffer(filePath);
            if (buffer != nullptr) {
                releasedBuffers.push_back(std::move(buffer));
            }
        }
    }
}

void stopVoiceRecord(VoiceRecord& record) {
    if (record.sound != nullptr) {
        record.sound->stop();
        record.sound.reset();
    }
    const std::string filePath = std::move(record.filePath);
    record = {};
    std::shared_ptr<sf::SoundBuffer> releasedBuffer;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        releasedBuffer = releaseBuffer(filePath);
    }
}

void stopMusicRecord(MusicRecord& record) {
    if (record.music != nullptr) {
        record.music->stop();
        record.music.reset();
    }
    record = {};
}

}  // namespace

std::shared_ptr<sf::SoundBuffer> AudioManager::loadSound(
    const std::string& filePath) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    const auto iterator = soundBuffers.find(filePath);
    if (iterator != soundBuffers.end()) {
        return iterator->second;
    }
    auto buffer = std::make_shared<sf::SoundBuffer>();
    if (!buffer->loadFromFile(ludork::standard::pathFromUtf8(filePath))) {
        throw std::runtime_error("Failed to load sound buffer from file: " +
                                 filePath);
    }
    return buffer;
}

std::shared_ptr<sf::Sound> AudioManager::playSound(
    const std::string& filePath, const SoundFilter* filter,
    const std::shared_ptr<sf::Transformable>& parent) {
    requireLogicThreadAudioLifecycle();
    if (!SystemConfigBase::getSoundOn()) {
        return nullptr;
    }
    AudioCreationScope creation;
    AudioEffectAttacher effect;
    std::uint64_t generation = 0;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (shuttingDown) {
            return nullptr;
        }
        creation.activate();
        effect = soundEffect;
        generation = soundGeneration;
    }
    const std::shared_ptr<sf::SoundBuffer> buffer = loadSound(filePath);
    const std::shared_ptr<ManagedSound> managedSound =
        std::make_shared<ManagedSound>(buffer);
    std::shared_ptr<AudioEffectControl> effectControl =
        attachEffect(*managedSound, effect);
    const float baseVolume =
        filter == nullptr ? managedSound->getVolume()
                          : filter->volume.value_or(managedSound->getVolume());
    const float basePitch =
        filter == nullptr ? 1.0f : filter->pitch.value_or(1.0f);
    if (filter != nullptr) {
        applySoundSettings(*managedSound, *filter);
    }
    if (parent != nullptr) {
        managedSound->setPosition(positionOf(parent));
    }
    if (parent == nullptr && !isSpatial(filter)) {
        managedSound->setSpatializationEnabled(false);
    }
    managedSound->setVolume(baseVolume * SystemConfigBase::getSoundVolume() /
                            100.0f);
    managedSound->setPitch(basePitch * TimeManager::getSpeed());
    managedSound->play();
    bool accepted = false;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (!shuttingDown && generation == soundGeneration) {
            retainBuffer(filePath, buffer);
            sounds.push_back({managedSound, filePath, parent, baseVolume,
                              basePitch, std::move(effectControl)});
            accepted = true;
        }
    }
    if (!accepted) {
        managedSound->stop();
        return nullptr;
    }
    return managedSound;
}

void AudioManager::setSoundParent(
    const std::shared_ptr<sf::Sound>& sound,
    const std::shared_ptr<sf::Transformable>& parent) {
    if (sound == nullptr) {
        return;
    }
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        SoundRecord* record = findSoundRecord(sound.get());
        if (record != nullptr) {
            record->parent = parent;
        }
    }
    if (parent != nullptr) {
        sound->setPosition(positionOf(parent));
    }
}

std::shared_ptr<sf::Sound> AudioManager::playVoice(
    const std::string& filePath, const SoundFilter* filter,
    const std::shared_ptr<sf::Transformable>& refActor, float minDistance) {
    requireLogicThreadAudioLifecycle();
    stopVoice();
    if (!SystemConfigBase::getVoiceOn()) {
        return nullptr;
    }
    AudioCreationScope creation;
    AudioEffectAttacher effect;
    std::uint64_t generation = 0;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (shuttingDown) {
            return nullptr;
        }
        creation.activate();
        effect = voiceEffect;
        generation = voiceGeneration;
    }
    const std::shared_ptr<sf::SoundBuffer> buffer = loadSound(filePath);
    const std::shared_ptr<ManagedSound> activeVoice =
        std::make_shared<ManagedSound>(buffer);
    std::shared_ptr<AudioEffectControl> effectControl =
        attachEffect(*activeVoice, effect);
    const float baseVolume =
        filter == nullptr ? activeVoice->getVolume()
                          : filter->volume.value_or(activeVoice->getVolume());
    if (filter != nullptr) {
        applySoundSettings(*activeVoice, *filter);
        activeVoice->setPitch(filter->pitch.value_or(1.0f) *
                              TimeManager::getSpeed());
    }
    if (refActor != nullptr) {
        activeVoice->setSpatializationEnabled(true);
        activeVoice->setRelativeToListener(false);
        activeVoice->setMinDistance(minDistance);
        activeVoice->setPosition(positionOf(refActor));
    }
    if (refActor == nullptr && !isSpatial(filter)) {
        activeVoice->setSpatializationEnabled(false);
    }
    activeVoice->setVolume(baseVolume * SystemConfigBase::getVoiceVolume() /
                           100.0f);
    activeVoice->play();
    bool accepted = false;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (!shuttingDown && generation == voiceGeneration &&
            voice.sound == nullptr) {
            retainBuffer(filePath, buffer);
            voice = {activeVoice, filePath, refActor, baseVolume,
                     std::move(effectControl)};
            accepted = true;
        }
    }
    if (!accepted) {
        activeVoice->stop();
        return nullptr;
    }
    return activeVoice;
}

void AudioManager::setVoiceRefActor(
    const std::shared_ptr<sf::Sound>& activeVoice,
    const std::shared_ptr<sf::Transformable>& refActor, float minDistance) {
    if (activeVoice == nullptr || refActor == nullptr) {
        return;
    }
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (voice.sound == activeVoice) {
            voice.refActor = refActor;
        }
    }
    activeVoice->setSpatializationEnabled(true);
    activeVoice->setRelativeToListener(false);
    activeVoice->setMinDistance(minDistance);
    activeVoice->setPosition(positionOf(refActor));
}

std::shared_ptr<sf::Music> AudioManager::playMusic(const std::string& musicType,
                                                   const std::string& filePath,
                                                   const MusicFilter* filter) {
    requireLogicThreadAudioLifecycle();
    stopMusic(musicType);
    const std::string alias = caseAlias(musicType);
    if (alias != musicType) {
        stopMusic(alias);
    }
    AudioCreationScope creation;
    AudioEffectAttacher effect;
    std::uint64_t generation = 0;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (shuttingDown) {
            return nullptr;
        }
        creation.activate();
        effect = musicEffect;
        generation = musicGenerations[musicType];
    }
    const std::shared_ptr<ManagedMusic> managedMusic =
        std::make_shared<ManagedMusic>();
    if (!managedMusic->openFromFile(ludork::standard::pathFromUtf8(filePath))) {
        throw std::runtime_error("Failed to load music from file: " + filePath);
    }
    std::shared_ptr<AudioEffectControl> effectControl =
        attachEffect(*managedMusic, effect);
    const float baseVolume =
        filter == nullptr ? managedMusic->getVolume()
                          : filter->volume.value_or(managedMusic->getVolume());
    if (filter != nullptr) {
        applyMusicSettings(*managedMusic, *filter);
        if (filter->pitch.has_value()) {
            managedMusic->setPitch(*filter->pitch);
        }
    } else {
        managedMusic->setSpatializationEnabled(false);
    }
    managedMusic->setVolume(
        SystemConfigBase::getMusicOn()
            ? baseVolume * SystemConfigBase::getMusicVolume() / 100.0f
            : 0.0f);
    managedMusic->play();
    bool accepted = false;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (!shuttingDown && generation == musicGenerations[musicType] &&
            musics.find(musicType) == musics.end()) {
            musics.emplace(musicType,
                           MusicRecord{managedMusic, filePath, baseVolume,
                                       std::move(effectControl)});
            accepted = true;
        }
    }
    if (!accepted) {
        managedMusic->stop();
        return nullptr;
    }
    return managedMusic;
}

void AudioManager::stopSound() {
    requireLogicThreadAudioLifecycle();
    std::vector<SoundRecord> stopped;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        ++soundGeneration;
        stopped = std::move(sounds);
        sounds.clear();
    }
    stopSoundRecords(stopped);
}

void AudioManager::stopVoice() {
    requireLogicThreadAudioLifecycle();
    VoiceRecord stopped;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        ++voiceGeneration;
        stopped = std::move(voice);
        voice = {};
    }
    stopVoiceRecord(stopped);
}

void AudioManager::stopMusic(const std::string& musicType) {
    requireLogicThreadAudioLifecycle();
    MusicRecord stopped;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        ++musicGenerations[musicType];
        const auto iterator = musics.find(musicType);
        if (iterator != musics.end()) {
            stopped = std::move(iterator->second);
            musics.erase(iterator);
        }
    }
    stopMusicRecord(stopped);
}

void AudioManager::applySoundVolumes() {
    if (!SystemConfigBase::getSoundOn()) {
        stopSound();
        return;
    }
    std::vector<std::pair<std::shared_ptr<sf::Sound>, float>> activeSounds;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        activeSounds.reserve(sounds.size());
        for (const SoundRecord& record : sounds) {
            activeSounds.emplace_back(record.sound, record.baseVolume);
        }
    }
    for (const auto& [sound, baseVolume] : activeSounds) {
        if (sound->getStatus() != sf::SoundSource::Status::Stopped) {
            sound->setVolume(baseVolume * SystemConfigBase::getSoundVolume() /
                             100.0f);
        }
    }
}

void AudioManager::applyVoiceVolumes() {
    if (!SystemConfigBase::getVoiceOn()) {
        stopVoice();
        return;
    }
    std::shared_ptr<sf::Sound> activeVoice;
    float baseVolume = 100.0f;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        activeVoice = voice.sound;
        baseVolume = voice.baseVolume;
    }
    if (activeVoice != nullptr &&
        activeVoice->getStatus() != sf::SoundSource::Status::Stopped) {
        activeVoice->setVolume(baseVolume * SystemConfigBase::getVoiceVolume() /
                               100.0f);
    }
}

void AudioManager::applyMusicVolumes() {
    std::vector<std::pair<std::shared_ptr<sf::Music>, float>> activeMusics;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        activeMusics.reserve(musics.size());
        for (const auto& [musicType, record] : musics) {
            static_cast<void>(musicType);
            activeMusics.emplace_back(record.music, record.baseVolume);
        }
    }
    for (const auto& [music, baseVolume] : activeMusics) {
        if (music->getStatus() == sf::SoundSource::Status::Stopped) {
            continue;
        }
        music->setVolume(SystemConfigBase::getMusicOn()
                             ? baseVolume * SystemConfigBase::getMusicVolume() /
                                   100.0f
                             : 0.0f);
    }
}

void AudioManager::updateAllSoundPositions() {
    std::vector<std::pair<std::shared_ptr<sf::Sound>,
                          std::shared_ptr<sf::Transformable>>>
        positionedSounds;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        positionedSounds.reserve(sounds.size());
        for (const SoundRecord& record : sounds) {
            if (record.parent != nullptr) {
                positionedSounds.emplace_back(record.sound, record.parent);
            }
        }
    }
    for (const auto& [sound, parent] : positionedSounds) {
        if (sound->getStatus() != sf::SoundSource::Status::Stopped) {
            sound->setPosition(positionOf(parent));
        }
    }
}

void AudioManager::updateAllVoicePositions() {
    std::shared_ptr<sf::Sound> activeVoice;
    std::shared_ptr<sf::Transformable> refActor;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        activeVoice = voice.sound;
        refActor = voice.refActor;
    }
    if (activeVoice != nullptr && refActor != nullptr &&
        activeVoice->getStatus() != sf::SoundSource::Status::Stopped) {
        activeVoice->setPosition(positionOf(refActor));
    }
}

void AudioManager::setSoundFilter(const std::shared_ptr<sf::Sound>& sound,
                                  const SoundFilter& filter) {
    if (sound == nullptr) {
        return;
    }
    SoundCategory category = SoundCategory::Unmanaged;
    float baseVolume = sound->getVolume();
    float basePitch = sound->getPitch();
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        SoundRecord* record = findSoundRecord(sound.get());
        if (record != nullptr) {
            category = SoundCategory::Sound;
            if (filter.volume.has_value()) {
                record->baseVolume = *filter.volume;
            }
            if (filter.pitch.has_value()) {
                record->basePitch = *filter.pitch;
            }
            baseVolume = record->baseVolume;
            basePitch = record->basePitch;
        } else if (voice.sound == sound) {
            category = SoundCategory::Voice;
            if (filter.volume.has_value()) {
                voice.baseVolume = *filter.volume;
            }
            baseVolume = voice.baseVolume;
        }
    }
    applySoundSettings(*sound, filter);
    if (filter.volume.has_value()) {
        if (category == SoundCategory::Sound) {
            if (!SystemConfigBase::getSoundOn()) {
                sound->stop();
            } else {
                sound->setVolume(baseVolume *
                                 SystemConfigBase::getSoundVolume() / 100.0f);
            }
        } else if (category == SoundCategory::Voice) {
            if (!SystemConfigBase::getVoiceOn()) {
                sound->stop();
            } else {
                sound->setVolume(baseVolume *
                                 SystemConfigBase::getVoiceVolume() / 100.0f);
            }
        } else {
            sound->setVolume(*filter.volume);
        }
    }
    if (category == SoundCategory::Sound) {
        sound->setPitch(basePitch * TimeManager::getSpeed());
    } else if (category == SoundCategory::Voice) {
        if (filter.pitch.has_value()) {
            sound->setPitch(*filter.pitch);
        }
    } else {
        sound->setPitch(filter.pitch.value_or(1.0f) * TimeManager::getSpeed());
    }
}

void AudioManager::setVoiceFilter(const std::shared_ptr<sf::Sound>& activeVoice,
                                  const SoundFilter& filter) {
    bool isActive = false;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        isActive = activeVoice != nullptr && activeVoice == voice.sound;
    }
    if (isActive) {
        setSoundFilter(activeVoice, filter);
    }
}

void AudioManager::setMusicFilter(const std::shared_ptr<sf::Music>& music,
                                  const MusicFilter& filter) {
    if (music == nullptr) {
        return;
    }
    float baseVolume = music->getVolume();
    bool isManaged = false;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        MusicRecord* record = findMusicRecord(music.get());
        if (record != nullptr) {
            isManaged = true;
            if (filter.volume.has_value()) {
                record->baseVolume = *filter.volume;
            }
            baseVolume = record->baseVolume;
        }
    }
    applyMusicSettings(*music, filter);
    if (filter.volume.has_value()) {
        music->setVolume(isManaged && SystemConfigBase::getMusicOn()
                             ? baseVolume * SystemConfigBase::getMusicVolume() /
                                   100.0f
                         : isManaged ? 0.0f
                                     : *filter.volume);
    }
    if (filter.pitch.has_value()) {
        music->setPitch(*filter.pitch);
    }
}

void AudioManager::setEffect(const std::string& audioType,
                             AudioEffectAttacher effect) {
    requireLogicThreadAudioLifecycle();
    AudioEffectAttacher previous;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        if (shuttingDown) {
            throw std::runtime_error("Audio manager is shutting down");
        }
        if (audioType == "Sound") {
            previous = std::move(soundEffect);
            soundEffect = std::move(effect);
        } else if (audioType == "Voice") {
            previous = std::move(voiceEffect);
            voiceEffect = std::move(effect);
        } else if (audioType == "Music") {
            previous = std::move(musicEffect);
            musicEffect = std::move(effect);
        } else {
            throw std::invalid_argument("Unknown audio type: " + audioType);
        }
    }
}

std::size_t AudioManager::getMemory() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    return sizeof(soundBuffers) + sizeof(soundBufferCounts) + sizeof(sounds) +
           sizeof(voice) + sizeof(musics) +
           soundBuffers.size() * sizeof(sf::SoundBuffer) +
           sounds.size() * sizeof(sf::Sound) +
           musics.size() * sizeof(sf::Music);
}

void AudioManager::initialize(lua_State* state) {
    ludork::global::audio::initializeAudioEffectLuaRuntime(state);
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    shuttingDown = false;
}

void AudioManager::update() {
    ludork::global::audio::throwDeferredAudioEffectError();
    updateAllSoundPositions();
    updateAllVoicePositions();
    std::vector<std::shared_ptr<ManagedSound>> observedSounds;
    std::shared_ptr<ManagedSound> observedVoice;
    std::vector<std::shared_ptr<ManagedMusic>> observedMusics;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        observedSounds.reserve(sounds.size());
        for (const SoundRecord& record : sounds) {
            observedSounds.push_back(record.sound);
        }
        observedVoice = voice.sound;
        observedMusics.reserve(musics.size());
        for (const auto& [musicType, record] : musics) {
            static_cast<void>(musicType);
            observedMusics.push_back(record.music);
        }
    }
    for (const std::shared_ptr<ManagedSound>& sound : observedSounds) {
        if (sound->getStatus() == sf::SoundSource::Status::Stopped &&
            !sound->wasExplicitlyStopped()) {
            sound->notifyNaturalInputEnded();
        }
    }
    if (observedVoice != nullptr &&
        observedVoice->getStatus() == sf::SoundSource::Status::Stopped &&
        !observedVoice->wasExplicitlyStopped()) {
        observedVoice->notifyNaturalInputEnded();
    }
    for (const std::shared_ptr<ManagedMusic>& music : observedMusics) {
        if (music->getStatus() == sf::SoundSource::Status::Stopped &&
            !music->wasExplicitlyStopped()) {
            music->notifyNaturalInputEnded();
        }
    }
    observedSounds.clear();
    observedVoice.reset();
    observedMusics.clear();
    std::vector<SoundRecord> finishedSounds;
    VoiceRecord finishedVoice;
    std::vector<MusicRecord> finishedMusics;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        for (auto iterator = sounds.begin(); iterator != sounds.end();) {
            if (!isFinished(*iterator->sound)) {
                if (iterator->sound->getStatus() !=
                    sf::SoundSource::Status::Stopped) {
                    iterator->sound->setPitch(iterator->basePitch *
                                              TimeManager::getSpeed());
                }
                ++iterator;
                continue;
            }
            if (!iterator->sound->wasExplicitlyStopped() &&
                iterator->sound.use_count() > 1) {
                ++iterator;
                continue;
            }
            finishedSounds.push_back(std::move(*iterator));
            iterator = sounds.erase(iterator);
        }
        if (voice.sound != nullptr && isFinished(*voice.sound) &&
            (voice.sound->wasExplicitlyStopped() ||
             voice.sound.use_count() == 1)) {
            finishedVoice = std::move(voice);
            voice = {};
        }
        for (auto iterator = musics.begin(); iterator != musics.end();) {
            if (isFinished(*iterator->second.music) &&
                (iterator->second.music->wasExplicitlyStopped() ||
                 iterator->second.music.use_count() == 1)) {
                finishedMusics.push_back(std::move(iterator->second));
                iterator = musics.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
    stopSoundRecords(finishedSounds);
    stopVoiceRecord(finishedVoice);
    for (MusicRecord& record : finishedMusics) {
        stopMusicRecord(record);
    }
}

void AudioManager::stopAll() {
    std::vector<SoundRecord> stoppedSounds;
    VoiceRecord stoppedVoice;
    std::vector<MusicRecord> stoppedMusics;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        ++soundGeneration;
        ++voiceGeneration;
        for (auto& [musicType, generation] : musicGenerations) {
            static_cast<void>(musicType);
            ++generation;
        }
        stoppedSounds = std::move(sounds);
        sounds.clear();
        stoppedVoice = std::move(voice);
        voice = {};
        stoppedMusics.reserve(musics.size());
        for (auto& [musicType, record] : musics) {
            static_cast<void>(musicType);
            stoppedMusics.push_back(std::move(record));
        }
        musics.clear();
    }
    stopSoundRecords(stoppedSounds);
    stopVoiceRecord(stoppedVoice);
    for (MusicRecord& record : stoppedMusics) {
        stopMusicRecord(record);
    }
}

void AudioManager::shutdown() noexcept {
    std::vector<SoundRecord> stoppedSounds;
    VoiceRecord stoppedVoice;
    std::vector<MusicRecord> stoppedMusics;
    AudioEffectAttacher stoppedSoundEffect;
    AudioEffectAttacher stoppedVoiceEffect;
    AudioEffectAttacher stoppedMusicEffect;
    std::unordered_map<std::string, std::shared_ptr<sf::SoundBuffer>>
        stoppedBuffers;
    {
        const std::lock_guard<std::recursive_mutex> lock(audioMutex);
        shuttingDown = true;
        ++soundGeneration;
        ++voiceGeneration;
        for (auto& [musicType, generation] : musicGenerations) {
            static_cast<void>(musicType);
            ++generation;
        }
        stoppedSounds = std::move(sounds);
        sounds.clear();
        stoppedVoice = std::move(voice);
        voice = {};
        stoppedMusics.reserve(musics.size());
        for (auto& [musicType, record] : musics) {
            static_cast<void>(musicType);
            stoppedMusics.push_back(std::move(record));
        }
        musics.clear();
        stoppedSoundEffect = std::move(soundEffect);
        stoppedVoiceEffect = std::move(voiceEffect);
        stoppedMusicEffect = std::move(musicEffect);
    }
    stopSoundRecords(stoppedSounds);
    stopVoiceRecord(stoppedVoice);
    for (MusicRecord& record : stoppedMusics) {
        stopMusicRecord(record);
    }
    stoppedSoundEffect = {};
    stoppedVoiceEffect = {};
    stoppedMusicEffect = {};
    {
        std::unique_lock<std::recursive_mutex> lock(audioMutex);
        audioCreationCondition.wait(lock, [] {
            return audioCreationsInFlight == 0;
        });
        soundBufferCounts.clear();
        stoppedBuffers = std::move(soundBuffers);
        soundBuffers.clear();
        musicGenerations.clear();
    }
    ludork::global::audio::shutdownAudioEffectLuaRuntime();
}
