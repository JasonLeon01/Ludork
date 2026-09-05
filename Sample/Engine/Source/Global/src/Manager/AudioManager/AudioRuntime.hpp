#pragma once

#include <Manager/ManagedAudioSource.hpp>

#include <SFML/Audio/AudioResource.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Transformable.hpp>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::global::audio_manager_impl {

class AudioDeviceLease final : private sf::AudioResource {
public:
    AudioDeviceLease() = default;
};

struct SoundRecord {
    std::shared_ptr<ludork::global::audio::ManagedSound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> parent;
    float baseVolume = 100.0f;
    float basePitch = 1.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct VoiceRecord {
    std::shared_ptr<ludork::global::audio::ManagedSound> sound;
    std::string filePath;
    std::shared_ptr<sf::Transformable> refActor;
    float baseVolume = 100.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct MusicRecord {
    std::shared_ptr<ludork::global::audio::ManagedMusic> music;
    std::string filePath;
    float baseVolume = 100.0f;
    std::shared_ptr<AudioEffectControl> effectControl;
};

struct AudioRuntime {
    std::unique_ptr<AudioDeviceLease> deviceLease;
    std::unordered_map<std::string, std::shared_ptr<sf::SoundBuffer>>
        soundBuffers;
    std::unordered_map<std::string, std::size_t> soundBufferCounts;
    std::vector<SoundRecord> sounds;
    VoiceRecord voice;
    std::unordered_map<std::string, MusicRecord> musics;
    ludork::global::audio::AudioEffectAttacher soundEffect;
    ludork::global::audio::AudioEffectAttacher voiceEffect;
    ludork::global::audio::AudioEffectAttacher musicEffect;
    std::uint64_t soundGeneration = 0;
    std::uint64_t voiceGeneration = 0;
    std::unordered_map<std::string, std::uint64_t> musicGenerations;
    bool shuttingDown = false;
    std::recursive_mutex mutex;
    std::condition_variable_any creationCondition;
    std::size_t creationsInFlight = 0;
};

AudioRuntime& audioRuntime();

}  // namespace ludork::global::audio_manager_impl
