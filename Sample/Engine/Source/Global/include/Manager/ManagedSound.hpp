#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Manager/AudioEffectControl.hpp>
#include <SFML/Audio/Sound.hpp>
#include <mutex>

namespace ludork::global::managed_audio_source_impl {
class EffectImpl;
class EffectStateToken;
}  // namespace ludork::global::managed_audio_source_impl

namespace ludork::global::audio {
class ManagedMusic;
class ManagedSound;
}  // namespace ludork::global::audio

namespace ludork::global::audio {

class ManagedSoundBufferOwner {
public:
    explicit ManagedSoundBufferOwner(
        const std::shared_ptr<const sf::SoundBuffer>& buffer);

protected:
    std::shared_ptr<const sf::SoundBuffer> buffer_;
};

class LUDORK_GLOBAL_API ManagedSound final : private ManagedSoundBufferOwner,
                                             public sf::Sound {
public:
    explicit ManagedSound(const std::shared_ptr<const sf::SoundBuffer>& buffer);
    ~ManagedSound() override;

    void play() override;
    void pause() override;
    void stop() override;
    void setEffectProcessor(EffectProcessor effectProcessor) override;

    void beginEffectAttachment(
        const std::shared_ptr<::AudioEffectControl>& control);
    void finishEffectAttachment();
    void abortEffectAttachment();
    void notifyNaturalInputEnded() noexcept;
    [[nodiscard]] bool isNaturalInputDrained() const noexcept;
    [[nodiscard]] bool wasExplicitlyStopped() const noexcept;

private:
    std::unique_ptr<managed_audio_source_impl::EffectImpl> effectImpl_;
    std::mutex mutationMutex_;
};

}  // namespace ludork::global::audio
