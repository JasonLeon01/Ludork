#pragma once

#include <BindAnnotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundSource.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

enum class AudioEffectState : std::uint8_t {
    Drained,
    TailPending,
    Cancelled
};

namespace ludork::global::managed_audio_source_impl {
class EffectRuntime;
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

namespace ludork::global::audio {

using AudioEffectAttacher = std::function<void(
    sf::SoundSource&, std::shared_ptr<::AudioEffectControl>, std::uint32_t)>;

[[nodiscard]] LUDORK_GLOBAL_API bool isManagedAudioCallbackThread() noexcept;

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
    std::unique_ptr<managed_audio_source_impl::EffectRuntime> effectState_;
    std::mutex mutationMutex_;
};

class LUDORK_GLOBAL_API ManagedMusic final : public sf::Music {
public:
    ManagedMusic();
    ~ManagedMusic() override;

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
    std::unique_ptr<managed_audio_source_impl::EffectRuntime> effectState_;
    std::mutex mutationMutex_;
};

}  // namespace ludork::global::audio
