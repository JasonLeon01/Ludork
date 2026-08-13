#pragma once

#include <BindAnnotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Audio/SoundSource.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

BIND_MODULE_PROPERTY(name = "AudioEffects", readonly = true,
                     metadata = false)
extern LUDORK_GLOBAL_API const std::unordered_map<std::string, std::string>
    AudioEffects;

namespace ludork::global::audio {

enum class AudioEffectFault {
    None,
    InvalidBuffer,
    SizeOverflow,
    UnsupportedChannelCount,
    ProcessingFailure
};

struct AudioEffectControl {
    std::atomic_bool cancelled{false};
    std::atomic_bool tailDrained{true};
    std::atomic<AudioEffectFault> fault{AudioEffectFault::None};
    std::atomic_bool faultReported{false};
};

struct AudioEffectBinding {
    sf::SoundSource::EffectProcessor processor;
    std::shared_ptr<AudioEffectControl> control;
};

LUDORK_GLOBAL_API void validateAudioEffect(std::string_view name);

LUDORK_GLOBAL_API bool hasAudioEffectProcessor(std::string_view name);

LUDORK_GLOBAL_API AudioEffectBinding createAudioEffect(
    std::string_view name, std::uint32_t sampleRate);

LUDORK_GLOBAL_API void cancelAudioEffect(
    const std::shared_ptr<AudioEffectControl>& control) noexcept;

LUDORK_GLOBAL_API bool isAudioEffectDrained(
    const std::shared_ptr<AudioEffectControl>& control) noexcept;

LUDORK_GLOBAL_API AudioEffectFault takeAudioEffectFault(
    const std::shared_ptr<AudioEffectControl>& control) noexcept;

LUDORK_GLOBAL_API const char* audioEffectFaultMessage(
    AudioEffectFault fault) noexcept;

}  // namespace ludork::global::audio
