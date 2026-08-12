#include <Manager/AudioManager.hpp>

#include <Filters/SoundFilter.hpp>
#include <Manager/TimeManager.hpp>
#include <SystemConfigBase.hpp>

#include <Utf8Path.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr float SpatialMinDistance = 64.0f;

struct SoundRecord {
    std::shared_ptr<sf::Sound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> parent;
    float baseVolume = 100.0f;
    float basePitch = 1.0f;
};

struct VoiceRecord {
    std::shared_ptr<sf::Sound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> refActor;
    float baseVolume = 100.0f;
};

struct MusicRecord {
    std::shared_ptr<sf::Music> music;
    std::string filePath;
    float baseVolume = 100.0f;
};

using EffectFactory = std::function<sf::SoundSource::EffectProcessor()>;

std::unordered_map<std::string, std::shared_ptr<sf::SoundBuffer>> soundBuffers;
std::unordered_map<std::string, std::size_t> soundBufferCounts;
std::vector<SoundRecord> sounds;
VoiceRecord voice;
std::unordered_map<std::string, MusicRecord> musics;
EffectFactory soundEffectFactory;
EffectFactory voiceEffectFactory;
EffectFactory musicEffectFactory;
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

void applyEffect(sf::SoundSource& source,
                 const EffectFactory& effectFactory) {
    if (!effectFactory) {
        source.setEffectProcessor({});
        return;
    }
    source.setEffectProcessor(effectFactory());
}

void setFilteredVolume(sf::SoundSource& source, float volume) {
    if (SoundRecord* record =
            dynamic_cast<sf::Sound*>(&source) == nullptr
                ? nullptr
                : findSoundRecord(static_cast<sf::Sound*>(&source));
        record != nullptr) {
        record->baseVolume = volume;
        if (!SystemConfigBase::getSoundOn()) {
            source.stop();
        } else {
            source.setVolume(volume * SystemConfigBase::getSoundVolume() /
                             100.0f);
        }
        return;
    }
    if (voice.sound.get() == &source) {
        voice.baseVolume = volume;
        if (!SystemConfigBase::getVoiceOn()) {
            source.stop();
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
    applyEffect(*sound, soundEffectFactory);
    retainBuffer(filePath, buffer);
    sounds.push_back({sound, filePath, parent, sound->getVolume(), 1.0f});
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
    applyEffect(*activeVoice, voiceEffectFactory);
    retainBuffer(filePath, buffer);
    if (filter != nullptr) {
        setSoundFilter(activeVoice, *filter);
    }
    voice = {activeVoice, filePath, refActor, activeVoice->getVolume()};
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
    applyEffect(*music, musicEffectFactory);
    if (filter != nullptr) {
        setMusicFilter(music, *filter);
    } else {
        music->setSpatializationEnabled(false);
    }
    MusicRecord record{music, filePath, music->getVolume()};
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
        record.sound->stop();
    }
    update();
}

void AudioManager::stopVoice() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (voice.sound == nullptr) {
        return;
    }
    voice.sound->stop();
    releaseBuffer(voice.filePath);
    voice = {};
}

void AudioManager::stopMusic(const std::string& musicType) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    const auto iterator = musics.find(musicType);
    if (iterator == musics.end()) {
        return;
    }
    iterator->second.music->stop();
    musics.erase(iterator);
}

void AudioManager::applySoundVolumes() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    for (SoundRecord& record : sounds) {
        if (record.sound->getStatus() == sf::SoundSource::Status::Stopped) {
            continue;
        }
        if (!SystemConfigBase::getSoundOn()) {
            record.sound->stop();
        } else {
            record.sound->setVolume(record.baseVolume *
                                    SystemConfigBase::getSoundVolume() /
                                    100.0f);
        }
    }
    update();
}

void AudioManager::applyVoiceVolumes() {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (voice.sound == nullptr ||
        voice.sound->getStatus() == sf::SoundSource::Status::Stopped) {
        return;
    }
    if (!SystemConfigBase::getVoiceOn()) {
        stopVoice();
    } else {
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

void AudioManager::setEffect(
    const std::string& audioType,
    std::function<sf::SoundSource::EffectProcessor()> effectFactory) {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
    if (audioType == "Sound") {
        soundEffectFactory = std::move(effectFactory);
        for (SoundRecord& record : sounds) {
            applyEffect(*record.sound, soundEffectFactory);
        }
        return;
    }
    if (audioType == "Voice") {
        voiceEffectFactory = std::move(effectFactory);
        if (voice.sound != nullptr) {
            applyEffect(*voice.sound, voiceEffectFactory);
        }
        return;
    }
    if (audioType == "Music") {
        musicEffectFactory = std::move(effectFactory);
        for (auto& [musicType, record] : musics) {
            static_cast<void>(musicType);
            applyEffect(*record.music, musicEffectFactory);
        }
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
    for (SoundRecord& record : sounds) {
        if (record.sound->getStatus() != sf::SoundSource::Status::Stopped) {
            record.sound->setPitch(record.basePitch * TimeManager::getSpeed());
            continue;
        }
        releaseBuffer(record.filePath);
    }
    sounds.erase(std::remove_if(sounds.begin(), sounds.end(),
                                [](const SoundRecord& record) {
                                    return record.sound->getStatus() ==
                                           sf::SoundSource::Status::Stopped;
                                }),
                 sounds.end());
    if (voice.sound != nullptr &&
        voice.sound->getStatus() == sf::SoundSource::Status::Stopped) {
        releaseBuffer(voice.filePath);
        voice = {};
    }
    for (auto iterator = musics.begin(); iterator != musics.end();) {
        if (iterator->second.music->getStatus() ==
            sf::SoundSource::Status::Stopped) {
            iterator = musics.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void AudioManager::shutdown() noexcept {
    const std::lock_guard<std::recursive_mutex> lock(audioMutex);
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
    soundEffectFactory = {};
    voiceEffectFactory = {};
    musicEffectFactory = {};
}
