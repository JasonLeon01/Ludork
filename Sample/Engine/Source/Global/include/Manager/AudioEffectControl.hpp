#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <SFML/Audio/SoundSource.hpp>

namespace ludork::global::managed_audio_source_impl {
class EffectImpl;
class EffectStateToken;
}  // namespace ludork::global::managed_audio_source_impl

namespace ludork::global::audio {
class ManagedMusic;
class ManagedSound;
}  // namespace ludork::global::audio

BIND_CLASS(metadata = false)
class LUDORK_GLOBAL_API AudioEffectControl
    : public std::enable_shared_from_this<AudioEffectControl> {
public:
    AudioEffectControl();
    ~AudioEffectControl();
    AudioEffectControl(const AudioEffectControl&) = delete;
    AudioEffectControl& operator=(const AudioEffectControl&) = delete;
    AudioEffectControl(AudioEffectControl&&) = delete;
    AudioEffectControl& operator=(AudioEffectControl&&) = delete;

    BIND_METHOD(metadata = false)
    bool isCancelled() const noexcept;

    BIND_METHOD(metadata = false)
    void beginTail() noexcept;

    BIND_METHOD(metadata = false)
    void finishTail() noexcept;

    BIND_METHOD(metadata = false)
    void attachLuaProcessor(sf::SoundSource& source, const std::string& name,
                            std::uint32_t sampleRate);

    void cancel() noexcept;

    [[nodiscard]] bool isDrained() const noexcept;

private:
    friend class ludork::global::audio::ManagedMusic;
    friend class ludork::global::audio::ManagedSound;

    [[nodiscard]] const std::shared_ptr<
        ludork::global::managed_audio_source_impl::EffectStateToken>&
    stateToken() const noexcept;

    std::shared_ptr<ludork::global::managed_audio_source_impl::EffectStateToken>
        state_;
};
