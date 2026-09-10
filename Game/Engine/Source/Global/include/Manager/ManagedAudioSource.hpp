#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <SFML/Audio/SoundSource.hpp>

enum class AudioEffectState : std::uint8_t {
    Drained,
    TailPending,
    Cancelled
};

class AudioEffectControl;

namespace ludork::global::audio {

using AudioEffectAttacher = std::function<void(
    sf::SoundSource&, std::shared_ptr<::AudioEffectControl>, std::uint32_t)>;

[[nodiscard]] LUDORK_GLOBAL_API bool isManagedAudioCallbackThread() noexcept;

}  // namespace ludork::global::audio
