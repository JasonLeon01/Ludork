#include <Manager/AudioManager.hpp>

#include <Filters/SoundFilter.hpp>
#include <Manager/AudioEffects.hpp>
#include <Manager/TimeManager.hpp>
#include <SystemConfigBase.hpp>

#include <Utf8Path.hpp>

#include <SFML/Audio/PlaybackDevice.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr float SpatialMinDistance = 64.0f;

using ludork::global::audio::AudioEffectBinding;
using ludork::global::audio::AudioEffectControl;
using ludork::global::audio::AudioEffectFault;

struct SoundRecord {
    std::shared_ptr<sf::Sound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> parent;
    float baseVolume = 100.0f;
    float basePitch = 1.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct VoiceRecord {
    std::shared_ptr<sf::Sound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> refActor;
    float baseVolume = 100.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct MusicRecord {
    std::shared_ptr<sf::Music> music;
    std::string filePath;
    float baseVolume = 100.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

std::unordered_map<std::string, std::shared_ptr<sf::SoundBuffer>> soundBuffers;
std::unordered_map<std::string, std::size_t> soundBufferCounts;
std::vector<SoundRecord> sounds;
VoiceRecord voice;
std::unordered_map<std::string, MusicRecord> musics;
std::string soundEffect = "nil";
std::string voiceEffect = "nil";
std::string musicEffect = "nil";
AudioEffectFault pendingAudioEffectFault = AudioEffectFault::None;
std::recursive_mutex audioMutex;

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

void releaseBuffer(const std::string& filePath) {
    const auto iterator = soundBufferCounts.find(filePath);
    if (iterator == soundBufferCounts.end()) {
        return;
    }
    if (iterator->second > 1) {
        --iterator->second;
        return;
    }
    soundBufferCounts.erase(iterator);
    soundBuffers.erase(filePath);
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

std::shared_ptr<AudioEffectControl> attachEffect(
    sf::SoundSource& source, const std::string& effect) {
    if (!ludork::global::audio::hasAudioEffectProcessor(effect)) {
        return nullptr;
    }
    const std::optional<std::uint32_t> sampleRate =
        sf::PlaybackDevice::getDeviceSampleRate();
    if (!sampleRate.has_value() || *sampleRate == 0) {
        throw std::runtime_error(
            "Audio playback device sample rate is unavailable");
    }
    AudioEffectBinding binding =
        ludork::global::audio::createAudioEffect(effect, *sampleRate);
    source.setEffectProcessor(std::move(binding.processor));
    return std::move(binding.control);
}

void stopManaged(
    sf::SoundSource& source,
    const std::shared_ptr<AudioEffectControl>& effectControl) {
    ludork::global::audio::cancelAudioEffect(effectControl);
    source.stop();
}

bool isFinished(
    const sf::SoundSource& source,
    const std::shared_ptr<AudioEffectControl>& effectControl) noexcept {
    return source.getStatus() == sf::SoundSource::Status::Stopped &&
           ludork::global::audio::isAudioEffectDrained(effectControl);
}

void captureEffectFault(
    const std::shared_ptr<AudioEffectControl>& effectControl) noexcept {
    const AudioEffectFault fault =
        ludork::global::audio::takeAudioEffectFault(effectControl);
    if (pendingAudioEffectFault == AudioEffectFault::None &&
        fault != AudioEffectFault::None) {
        pendingAudioEffectFault = fault;
    }
}

void setFilteredVolume(sf::SoundSource& source, float volume) {
    if (SoundRecord* record =
            dynamic_cast<sf::Sound*>(&source) == nullptr
                ? nullptr
                : findSoundRecord(static_cast<sf::Sound*>(&source));
        record != nullptr) {
        record->baseVolume = volume;
        if (!SystemConfigBase::getSoundOn()) {
            stopManaged(source, record->effectControl);
        } else {
            source.setVolume(volume * SystemConfigBase::getSoundVolume() /
                             100.0f);
        }
        return;
    }
    if (voice.sound.get() == &source) {
        voice.baseVolume = volume;
        if (!SystemConfigBase::getVoiceOn()) {
            stopManaged(source, voice.effectControl);
        } else {
            source.setVolume(volume * SystemConfigBase::getVoiceVolume() /
                             100.0f);
        }
        return;
    }
    MusicRecord* musicRecord =
        dynamic_cast<sf::Music*>(&source) == nullptr
            ? nullptr
            : findMusicRecord(static_cast<sf::Music*>(&source));
    if (musicRecord != nullptr) {
        musicRecord->baseVolume = volume;
        source.setVolume(SystemConfigBase::getMusicOn()
                             ? volume * SystemConfigBase::getMusicVolume() /
                                   100.0f
                             : 0.0f);
        return;
    }
    source.setVolume(volume);
}

void applyAudioFilter(sf::SoundSource& source, const SoundFilter& filter) {
    if (filter.pan.has_value()) {
        source.setPan(*filter.pan);
    }
    if (filter.volume.has_value()) {
        setFilteredVolume(source, *filter.volume);
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

bool isSpatial(const SoundFilter* filter) {
    return filter != nullptr && filter->spatial.value_or(false);
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
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (!SystemConfigBase::getSoundOn()) {
        return nullptr;
    }
    const std::shared_ptr<sf::SoundBuffer> buffer = loadSound(filePath);
    auto sound = std::make_shared<sf::Sound>(*buffer);
    std::shared_ptr<AudioEffectControl> effectControl =
        attachEffect(*sound, soundEffect);
    retainBuffer(filePath, buffer);
    sounds.push_back({sound, filePath, parent, sound->getVolume(), 1.0f,
                      std::move(effectControl)});
    if (parent != nullptr) {
        setSoundParent(sound, parent);
    }
    if (filter != nullptr) {
        setSoundFilter(sound, *filter);
    } else {
        sound->setPitch(TimeManager::getSpeed());
    }
    if (parent == nullptr && !isSpatial(filter)) {
        sound->setSpatializationEnabled(false);
    }
    SoundRecord* record = findSoundRecord(sound.get());
    if ((filter == nullptr || !filter->volume.has_value()) &&
        SystemConfigBase::getSoundVolume() != 100.0f) {
        sound->setVolume(record->baseVolume *
                         SystemConfigBase::getSoundVolume() / 100.0f);
    }
    sound->play();
    return sound;
}

void AudioManager::setSoundParent(
    const std::shared_ptr<sf::Sound>& sound,
    const std::shared_ptr<sf::Transformable>& parent) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (sound == nullptr) {
        return;
    }
    SoundRecord* record = findSoundRecord(sound.get());
    if (record != nullptr) {
        record->parent = parent;
    }
    if (parent != nullptr) {
        sound->setPosition(positionOf(parent));
    }
}

std::shared_ptr<sf::Sound> AudioManager::playVoice(
    const std::string& filePath, const SoundFilter* filter,
    const std::shared_ptr<sf::Transformable>& refActor, float minDistance) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (!SystemConfigBase::getVoiceOn()) {
        stopVoice();
        return nullptr;
    }
    stopVoice();
    const std::shared_ptr<sf::SoundBuffer> buffer = loadSound(filePath);
    auto activeVoice = std::make_shared<sf::Sound>(*buffer);
    std::shared_ptr<AudioEffectControl> effectControl =
        attachEffect(*activeVoice, voiceEffect);
    retainBuffer(filePath, buffer);
    if (filter != nullptr) {
        setSoundFilter(activeVoice, *filter);
    }
    voice = {activeVoice, filePath, refActor, activeVoice->getVolume(),
             std::move(effectControl)};
    if (refActor != nullptr) {
        setVoiceRefActor(activeVoice, refActor, minDistance);
    }
    if (refActor == nullptr && !isSpatial(filter)) {
        activeVoice->setSpatializationEnabled(false);
    }
    if (SystemConfigBase::getVoiceVolume() != 100.0f) {
        activeVoice->setVolume(voice.baseVolume *
                               SystemConfigBase::getVoiceVolume() / 100.0f);
    }
    activeVoice->play();
    return activeVoice;
}

void AudioManager::setVoiceRefActor(
    const std::shared_ptr<sf::Sound>& activeVoice,
    const std::shared_ptr<sf::Transformable>& refActor, float minDistance) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (activeVoice == nullptr || refActor == nullptr) {
        return;
    }
    if (voice.sound == activeVoice) {
        voice.refActor = refActor;
    }
    activeVoice->setSpatializationEnabled(true);
    activeVoice->setRelativeToListener(false);
    activeVoice->setMinDistance(minDistance);
    activeVoice->setPosition(positionOf(refActor));
}

std::shared_ptr<sf::Music> AudioManager::playMusic(const std::string& musicType,
                                                   const std::string& filePath,
                                                   const MusicFilter* filter) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    stopMusic(musicType);
    const std::string alias = caseAlias(musicType);
    if (alias != musicType) {
        stopMusic(alias);
    }
    auto music = std::make_shared<sf::Music>();
    if (!music->openFromFile(ludork::standard::pathFromUtf8(filePath))) {
        throw std::runtime_error("Failed to load music from file: " + filePath);
    }
    std::shared_ptr<AudioEffectControl> effectControl =
        attachEffect(*music, musicEffect);
    if (filter != nullptr) {
        setMusicFilter(music, *filter);
    } else {
        music->setSpatializationEnabled(false);
    }
    MusicRecord record{music, filePath, music->getVolume(),
                       std::move(effectControl)};
    music->setVolume(SystemConfigBase::getMusicOn()
                         ? record.baseVolume *
                               SystemConfigBase::getMusicVolume() / 100.0f
                         : 0.0f);
    musics[musicType] = record;
    music->play();
    return music;
}

void AudioManager::stopSound() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (SoundRecord& record : sounds) {
        stopManaged(*record.sound, record.effectControl);
        captureEffectFault(record.effectControl);
    }
    update();
}

void AudioManager::stopVoice() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (voice.sound == nullptr) {
        return;
    }
    stopManaged(*voice.sound, voice.effectControl);
    captureEffectFault(voice.effectControl);
    std::string filePath = std::move(voice.filePath);
    voice = {};
    releaseBuffer(filePath);
}

void AudioManager::stopMusic(const std::string& musicType) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    const auto iterator = musics.find(musicType);
    if (iterator == musics.end()) {
        return;
    }
    stopManaged(*iterator->second.music, iterator->second.effectControl);
    captureEffectFault(iterator->second.effectControl);
    musics.erase(iterator);
}

void AudioManager::applySoundVolumes() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (SoundRecord& record : sounds) {
        if (!SystemConfigBase::getSoundOn()) {
            stopManaged(*record.sound, record.effectControl);
        } else if (record.sound->getStatus() !=
                   sf::SoundSource::Status::Stopped) {
            record.sound->setVolume(record.baseVolume *
                                    SystemConfigBase::getSoundVolume() /
                                    100.0f);
        }
    }
    update();
}

void AudioManager::applyVoiceVolumes() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (voice.sound == nullptr) {
        return;
    }
    if (!SystemConfigBase::getVoiceOn()) {
        stopVoice();
    } else if (voice.sound->getStatus() !=
               sf::SoundSource::Status::Stopped) {
        voice.sound->setVolume(voice.baseVolume *
                               SystemConfigBase::getVoiceVolume() / 100.0f);
    }
}

void AudioManager::applyMusicVolumes() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (auto& [musicType, record] : musics) {
        static_cast<void>(musicType);
        if (record.music->getStatus() == sf::SoundSource::Status::Stopped) {
            continue;
        }
        record.music->setVolume(SystemConfigBase::getMusicOn()
                                    ? record.baseVolume *
                                          SystemConfigBase::getMusicVolume() /
                                          100.0f
                                    : 0.0f);
    }
}

void AudioManager::updateAllSoundPositions() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (SoundRecord& record : sounds) {
        if (record.parent != nullptr &&
            record.sound->getStatus() != sf::SoundSource::Status::Stopped) {
            record.sound->setPosition(positionOf(record.parent));
        }
    }
}

void AudioManager::updateAllVoicePositions() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (voice.sound != nullptr && voice.refActor != nullptr &&
        voice.sound->getStatus() != sf::SoundSource::Status::Stopped) {
        voice.sound->setPosition(positionOf(voice.refActor));
    }
}

void AudioManager::setSoundFilter(const std::shared_ptr<sf::Sound>& sound,
                                  const SoundFilter& filter) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (sound == nullptr) {
        return;
    }
    if (filter.loop.has_value()) {
        sound->setLooping(*filter.loop);
    }
    if (filter.offset.has_value()) {
        sound->setPlayingOffset(*filter.offset);
    }
    applyAudioFilter(*sound, filter);
    SoundRecord* record = findSoundRecord(sound.get());
    if (record != nullptr) {
        record->basePitch = filter.pitch.value_or(record->basePitch);
        sound->setPitch(record->basePitch * TimeManager::getSpeed());
    } else if (voice.sound == sound && filter.pitch.has_value()) {
        sound->setPitch(*filter.pitch);
    } else {
        sound->setPitch(filter.pitch.value_or(1.0f) * TimeManager::getSpeed());
    }
}

void AudioManager::setVoiceFilter(const std::shared_ptr<sf::Sound>& activeVoice,
                                  const SoundFilter& filter) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (activeVoice == voice.sound) {
        setSoundFilter(activeVoice, filter);
    }
}

void AudioManager::setMusicFilter(const std::shared_ptr<sf::Music>& music,
                                  const MusicFilter& filter) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (music == nullptr) {
        return;
    }
    if (filter.loop.has_value()) {
        music->setLooping(*filter.loop);
    }
    if (filter.offset.has_value()) {
        music->setPlayingOffset(*filter.offset);
    }
    if (filter.loopPoint.has_value()) {
        music->setLoopPoints(*filter.loopPoint);
    }
    applyAudioFilter(*music, filter);
    if (filter.pitch.has_value()) {
        music->setPitch(*filter.pitch);
    }
    music->setSpatializationEnabled(false);
}

void AudioManager::setEffect(const std::string& audioType,
                             const std::string& effect) {
    ludork::global::audio::validateAudioEffect(effect);
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (audioType == "Sound") {
        soundEffect = effect;
        return;
    }
    if (audioType == "Voice") {
        voiceEffect = effect;
        return;
    }
    if (audioType == "Music") {
        musicEffect = effect;
        return;
    }
    throw std::invalid_argument("Unknown audio type: " + audioType);
}

std::size_t AudioManager::getMemory() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    return sizeof(soundBuffers) + sizeof(soundBufferCounts) + sizeof(sounds) +
           sizeof(voice) + sizeof(musics) +
           soundBuffers.size() * sizeof(sf::SoundBuffer) +
           sounds.size() * sizeof(sf::Sound) +
           musics.size() * sizeof(sf::Music);
}

void AudioManager::update() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    updateAllSoundPositions();
    updateAllVoicePositions();
    for (auto iterator = sounds.begin(); iterator != sounds.end();) {
        const AudioEffectFault fault =
            ludork::global::audio::takeAudioEffectFault(
                iterator->effectControl);
        if (fault != AudioEffectFault::None) {
            if (pendingAudioEffectFault == AudioEffectFault::None) {
                pendingAudioEffectFault = fault;
            }
            stopManaged(*iterator->sound, iterator->effectControl);
        }
        if (!isFinished(*iterator->sound, iterator->effectControl)) {
            if (iterator->sound->getStatus() !=
                sf::SoundSource::Status::Stopped) {
                iterator->sound->setPitch(iterator->basePitch *
                                          TimeManager::getSpeed());
            }
            ++iterator;
            continue;
        }
        stopManaged(*iterator->sound, iterator->effectControl);
        captureEffectFault(iterator->effectControl);
        std::string filePath = std::move(iterator->filePath);
        iterator = sounds.erase(iterator);
        releaseBuffer(filePath);
    }
    if (voice.sound != nullptr) {
        const AudioEffectFault fault =
            ludork::global::audio::takeAudioEffectFault(voice.effectControl);
        if (fault != AudioEffectFault::None) {
            if (pendingAudioEffectFault == AudioEffectFault::None) {
                pendingAudioEffectFault = fault;
            }
            stopManaged(*voice.sound, voice.effectControl);
        }
        if (isFinished(*voice.sound, voice.effectControl)) {
            stopManaged(*voice.sound, voice.effectControl);
            captureEffectFault(voice.effectControl);
            std::string filePath = std::move(voice.filePath);
            voice = {};
            releaseBuffer(filePath);
        }
    }
    for (auto iterator = musics.begin(); iterator != musics.end();) {
        const AudioEffectFault fault =
            ludork::global::audio::takeAudioEffectFault(
                iterator->second.effectControl);
        if (fault != AudioEffectFault::None) {
            if (pendingAudioEffectFault == AudioEffectFault::None) {
                pendingAudioEffectFault = fault;
            }
            stopManaged(*iterator->second.music,
                        iterator->second.effectControl);
        }
        if (isFinished(*iterator->second.music,
                       iterator->second.effectControl)) {
            stopManaged(*iterator->second.music,
                        iterator->second.effectControl);
            captureEffectFault(iterator->second.effectControl);
            iterator = musics.erase(iterator);
        } else {
            ++iterator;
        }
    }
    if (pendingAudioEffectFault != AudioEffectFault::None) {
        const AudioEffectFault fault = pendingAudioEffectFault;
        pendingAudioEffectFault = AudioEffectFault::None;
        throw std::runtime_error(
            ludork::global::audio::audioEffectFaultMessage(fault));
    }
}

void AudioManager::shutdown() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (SoundRecord& record : sounds) {
        ludork::global::audio::cancelAudioEffect(record.effectControl);
    }
    ludork::global::audio::cancelAudioEffect(voice.effectControl);
    for (auto& [musicType, record] : musics) {
        static_cast<void>(musicType);
        ludork::global::audio::cancelAudioEffect(record.effectControl);
    }
    for (SoundRecord& record : sounds) {
        record.sound->stop();
    }
    if (voice.sound != nullptr) {
        voice.sound->stop();
    }
    for (auto& [musicType, record] : musics) {
        static_cast<void>(musicType);
        record.music->stop();
    }
    sounds.clear();
    voice = {};
    musics.clear();
    soundBufferCounts.clear();
    soundBuffers.clear();
    soundEffect = "nil";
    voiceEffect = "nil";
    musicEffect = "nil";
    pendingAudioEffectFault = AudioEffectFault::None;
}
