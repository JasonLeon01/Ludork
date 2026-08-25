#pragma once

#include <BindAnnotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundSource.hpp>

#include <atomic>
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

BIND_CLASS(metadata = false)
class LUDORK_GLOBAL_API AudioEffectControl
    : public std::enable_shared_from_this<AudioEffectControl> {
public:
    BIND_METHOD(metadata = false)
    bool isCancelled() const noexcept;

    BIND_METHOD(metadata = false)
    void beginTail() noexcept;

    BIND_METHOD(metadata = false)
    void finishTail() noexcept;

    BIND_METHOD(metadata = false)
    void attachLuaProcessor(sf::SoundSource& source, const std::string& name,
                            std::uint32_t sampleRate);

    BIND_IGNORE()
    void cancel() noexcept;

    BIND_IGNORE()
    [[nodiscard]] bool isDrained() const noexcept;

private:
    std::atomic<AudioEffectState> state_{AudioEffectState::Drained};
};

namespace ludork::global::audio {

using AudioEffectAttacher = std::function<void(
    sf::SoundSource&, std::shared_ptr<::AudioEffectControl>, std::uint32_t)>;

[[nodiscard]] LUDORK_GLOBAL_API bool isManagedAudioCallbackThread() noexcept;

class ManagedAudioEffectState;

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
    std::unique_ptr<ManagedAudioEffectState> effectState_;
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
    std::unique_ptr<ManagedAudioEffectState> effectState_;
    std::mutex mutationMutex_;
};

}  // namespace ludork::global::audio
