#pragma once

#include "EffectStateToken.hpp"

#include <SFML/Audio/SoundSource.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace ludork::global::managed_audio_source_impl {

enum class ManagedInputState : std::uint8_t {
    Dormant,
    Starting,
    Playing,
    NaturalEnding,
    NaturalEnded,
    ExplicitEnded
};

class ProcessorGeneration;

class RetiredProcessorGenerations {
public:
    RetiredProcessorGenerations() = default;
    ~RetiredProcessorGenerations();
    RetiredProcessorGenerations(const RetiredProcessorGenerations&) = delete;
    RetiredProcessorGenerations& operator=(const RetiredProcessorGenerations&) =
        delete;
    RetiredProcessorGenerations(RetiredProcessorGenerations&& other) noexcept;
    RetiredProcessorGenerations& operator=(
        RetiredProcessorGenerations&& other) noexcept;

private:
    friend class EffectRuntime;

    explicit RetiredProcessorGenerations(
        std::shared_ptr<ProcessorGeneration> head) noexcept;
    void clear() noexcept;

    std::shared_ptr<ProcessorGeneration> head_;
};

class EffectRuntime {
public:
    explicit EffectRuntime(const sf::SoundSource& source) noexcept;

    [[nodiscard]] sf::SoundSource::EffectProcessor makeTrampoline();
    void begin(const std::shared_ptr<EffectStateToken>& control);
    void replace(sf::SoundSource::EffectProcessor effectProcessor);
    void finish();
    void abort() noexcept;
    void cancel() noexcept;
    void clear() noexcept;
    void preparePlaying() noexcept;
    void markPlaying() noexcept;
    void notifyNaturalInputEnded() noexcept;
    [[nodiscard]] bool isNaturalInputDrained() const noexcept;
    [[nodiscard]] RetiredProcessorGenerations takeAllRetired() noexcept;
    void waitForCallbacks() const noexcept;
    [[nodiscard]] bool wasExplicitlyStopped() const noexcept;

private:
    void completeNaturalInput(ProcessorGeneration* generation) noexcept;
    static void waitUntilInactive(
        const std::shared_ptr<ProcessorGeneration>& generation) noexcept;
    [[nodiscard]] std::shared_ptr<ProcessorGeneration> retireCurrentLocked(
        std::shared_ptr<ProcessorGeneration> replacement) noexcept;

    std::atomic<ProcessorGeneration*> processor_{nullptr};
    const sf::SoundSource* source_;
    std::shared_ptr<ProcessorGeneration> processorOwner_;
    std::shared_ptr<ProcessorGeneration> retired_;
    mutable std::mutex attachmentMutex_;
    std::shared_ptr<EffectStateToken> pendingControl_;
    std::size_t attachmentCount_ = 0;
    std::atomic_bool explicitlyStopped_{false};
    std::atomic_size_t callbacksInFlight_{0};
    std::atomic<ManagedInputState> inputState_{ManagedInputState::Dormant};
    bool naturalStopObservedByManager_ = false;
    bool stopping_ = false;
};

}  // namespace ludork::global::managed_audio_source_impl
