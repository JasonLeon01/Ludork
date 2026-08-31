#include "EffectRuntime.hpp"

#include "AudioCallbackRuntime.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ludork::global::managed_audio_source_impl {

class ProcessorGeneration {
public:
    static constexpr std::uint32_t CancelledBit = 1U << 31U;
    static constexpr std::uint32_t ActivityMask = CancelledBit - 1U;

    std::shared_ptr<EffectStateToken> control;
    sf::SoundSource::EffectProcessor processor;
    std::shared_ptr<ProcessorGeneration> retiredNext;
    std::atomic_uint32_t gate{0};
    std::atomic_bool endOfStreamComplete{false};
    bool requiresEndOfStreamAcknowledgement = false;

    [[nodiscard]] bool tryAcquireActivity() noexcept {
        std::uint32_t state = gate.load(std::memory_order_acquire);
        while ((state & CancelledBit) == 0) {
            if ((state & ActivityMask) == ActivityMask) {
                return false;
            }
            if (gate.compare_exchange_weak(state, state + 1,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }

    void releaseActivity() noexcept {
        const std::uint32_t previous =
            gate.fetch_sub(1, std::memory_order_acq_rel);
        if ((previous & ActivityMask) == 1) {
            gate.notify_all();
        }
    }

    void cancel() noexcept {
        gate.fetch_or(CancelledBit, std::memory_order_acq_rel);
        control->cancel();
    }

    void waitUntilInactive() const noexcept {
        std::uint32_t state = gate.load(std::memory_order_acquire);
        while ((state & ActivityMask) != 0) {
            gate.wait(state, std::memory_order_acquire);
            state = gate.load(std::memory_order_acquire);
        }
    }
};

static_assert(std::atomic<ProcessorGeneration*>::is_always_lock_free);
static_assert(std::atomic_uint32_t::is_always_lock_free);
static_assert(std::atomic_size_t::is_always_lock_free);
static_assert(std::atomic<ManagedInputState>::is_always_lock_free);

RetiredProcessorGenerations::RetiredProcessorGenerations(
    std::shared_ptr<ProcessorGeneration> head) noexcept
    : head_(std::move(head)) {}

RetiredProcessorGenerations::~RetiredProcessorGenerations() {
    clear();
}

RetiredProcessorGenerations::RetiredProcessorGenerations(
    RetiredProcessorGenerations&& other) noexcept
    : head_(std::move(other.head_)) {}

RetiredProcessorGenerations& RetiredProcessorGenerations::operator=(
    RetiredProcessorGenerations&& other) noexcept {
    if (this != &other) {
        clear();
        head_ = std::move(other.head_);
    }
    return *this;
}

void RetiredProcessorGenerations::clear() noexcept {
    while (head_ != nullptr) {
        std::shared_ptr<ProcessorGeneration> next =
            std::move(head_->retiredNext);
        head_.reset();
        head_ = std::move(next);
    }
}

EffectRuntime::EffectRuntime(const sf::SoundSource& source) noexcept
    : source_(&source) {}

sf::SoundSource::EffectProcessor EffectRuntime::makeTrampoline() {
    return [this](const float* inputFrames, unsigned int& inputFrameCount,
                  float* outputFrames, unsigned int& outputFrameCount,
                  unsigned int frameChannelCount) {
        enterCallback();
        callbacksInFlight_.fetch_add(1, std::memory_order_acq_rel);
        struct CallbackScope {
            EffectRuntime* state;

            ~CallbackScope() {
                if (state->callbacksInFlight_.fetch_sub(
                        1, std::memory_order_acq_rel) == 1) {
                    state->callbacksInFlight_.notify_all();
                }
                leaveCallback();
            }
        } callbackScope{this};
        ManagedInputState inputState =
            inputState_.load(std::memory_order_acquire);
        bool completesNaturalInput = false;
        bool startsNaturalEndOfStream = false;
        if (inputState == ManagedInputState::Playing &&
            source_->getStatus() == sf::SoundSource::Status::Stopped) {
            if (inputState_.compare_exchange_strong(
                    inputState, ManagedInputState::NaturalEnding,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                inputState = ManagedInputState::NaturalEnding;
                completesNaturalInput =
                    inputFrames != nullptr && inputFrameCount != 0;
                startsNaturalEndOfStream = !completesNaturalInput;
            }
        }
        if (inputState == ManagedInputState::Dormant ||
            inputState == ManagedInputState::Starting ||
            inputState == ManagedInputState::ExplicitEnded ||
            (inputState == ManagedInputState::NaturalEnding &&
             !completesNaturalInput && !startsNaturalEndOfStream)) {
            returnNoInput(inputFrameCount, outputFrameCount);
            return;
        }
        const bool endOfStream =
            inputState == ManagedInputState::NaturalEnded ||
            startsNaturalEndOfStream;
        const unsigned int availableInput = inputFrameCount;
        const unsigned int outputCapacity = outputFrameCount;
        ProcessorGeneration* const generation =
            processor_.load(std::memory_order_acquire);
        if (generation == nullptr) {
            if (endOfStream) {
                returnNoInput(inputFrameCount, outputFrameCount);
            } else {
                dryBypass(inputFrames, inputFrameCount, outputFrames,
                          outputFrameCount, frameChannelCount);
            }
            if (completesNaturalInput || startsNaturalEndOfStream) {
                completeNaturalInput(generation);
            }
            return;
        }
        if (startsNaturalEndOfStream) {
            completeNaturalInput(generation);
        }
        if (endOfStream &&
            generation->endOfStreamComplete.load(std::memory_order_acquire)) {
            returnNoInput(inputFrameCount, outputFrameCount);
            return;
        }
        if (!generation->tryAcquireActivity()) {
            if (endOfStream) {
                returnNoInput(inputFrameCount, outputFrameCount);
            } else {
                silenceCancelledBlock(inputFrames, inputFrameCount,
                                      outputFrames, outputFrameCount,
                                      frameChannelCount);
            }
            if (completesNaturalInput) {
                completeNaturalInput(generation);
            }
            return;
        }
        struct ActivityScope {
            ProcessorGeneration* generation;

            ~ActivityScope() {
                generation->releaseActivity();
            }
        } activityScope{generation};
        if (generation->control->isCancelled()) {
            if (endOfStream) {
                returnNoInput(inputFrameCount, outputFrameCount);
            } else {
                silenceCancelledBlock(inputFrames, inputFrameCount,
                                      outputFrames, outputFrameCount,
                                      frameChannelCount);
            }
            if (completesNaturalInput) {
                completeNaturalInput(generation);
            }
            return;
        }
        if (endOfStream && generation->requiresEndOfStreamAcknowledgement) {
            generation->control->beginTail();
        }
        if (endOfStream) {
            inputFrameCount = 0;
        }
        generation->processor(endOfStream ? nullptr : inputFrames,
                              inputFrameCount, outputFrames, outputFrameCount,
                              frameChannelCount);
        if (completesNaturalInput) {
            completeNaturalInput(generation);
        }
        if (generation->control->isCancelled()) {
            if (endOfStream) {
                returnNoInput(inputFrameCount, outputFrameCount);
                return;
            }
            inputFrameCount = availableInput;
            outputFrameCount = outputCapacity;
            silenceCancelledBlock(inputFrames, inputFrameCount, outputFrames,
                                  outputFrameCount, frameChannelCount);
            return;
        }
        if (endOfStream && inputFrameCount == 0 && outputFrameCount == 0 &&
            (!generation->requiresEndOfStreamAcknowledgement ||
             generation->control->isDrained())) {
            generation->control->finishTail();
            generation->endOfStreamComplete.store(true,
                                                  std::memory_order_release);
        }
    };
}

void EffectRuntime::begin(const std::shared_ptr<EffectStateToken>& control) {
    const std::lock_guard<std::mutex> lock(attachmentMutex_);
    if (stopping_) {
        throw std::logic_error(
            "Audio effect attachment cannot begin while stopping");
    }
    if (pendingControl_ != nullptr) {
        throw std::logic_error("Audio effect attachment is already active");
    }
    if (control == nullptr) {
        throw std::invalid_argument("Audio effect control is required");
    }
    if (processorOwner_ != nullptr) {
        throw std::logic_error(
            "Audio effect attachment requires an empty processor");
    }
    pendingControl_ = control;
    attachmentCount_ = 0;
}

void EffectRuntime::replace(sf::SoundSource::EffectProcessor effectProcessor) {
    std::shared_ptr<ProcessorGeneration> cancelled;
    {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        if (stopping_) {
            throw std::logic_error(
                "Audio effect processor cannot be replaced while stopping");
        }
        std::shared_ptr<EffectStateToken> control;
        const bool requiresEndOfStreamAcknowledgement =
            pendingControl_ != nullptr;
        if (pendingControl_ != nullptr) {
            if (!effectProcessor) {
                throw std::invalid_argument(
                    "Audio effect attacher installed an empty processor");
            }
            if (attachmentCount_ != 0) {
                throw std::logic_error(
                    "Audio effect attacher installed more than one processor");
            }
            control = pendingControl_;
        } else if (effectProcessor) {
            control = std::make_shared<EffectStateToken>();
        }

        std::shared_ptr<ProcessorGeneration> replacement;
        if (effectProcessor) {
            replacement = std::make_shared<ProcessorGeneration>();
            replacement->control = std::move(control);
            replacement->processor = std::move(effectProcessor);
            replacement->requiresEndOfStreamAcknowledgement =
                requiresEndOfStreamAcknowledgement;
            if (inputState_.load(std::memory_order_acquire) ==
                    ManagedInputState::NaturalEnded &&
                replacement->requiresEndOfStreamAcknowledgement) {
                replacement->control->beginTail();
            }
        }
        cancelled = retireCurrentLocked(std::move(replacement));
        if (pendingControl_ != nullptr) {
            attachmentCount_ = 1;
        }
    }
    waitUntilInactive(cancelled);
}

void EffectRuntime::finish() {
    const std::lock_guard<std::mutex> lock(attachmentMutex_);
    const std::shared_ptr<ProcessorGeneration>& generation = processorOwner_;
    if (pendingControl_ == nullptr || attachmentCount_ != 1 ||
        generation == nullptr || generation->control != pendingControl_) {
        throw std::logic_error(
            "Audio effect attacher did not install exactly one processor");
    }
    pendingControl_.reset();
    attachmentCount_ = 0;
}

void EffectRuntime::abort() noexcept {
    std::shared_ptr<ProcessorGeneration> cancelled;
    {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        explicitlyStopped_.store(true, std::memory_order_release);
        inputState_.store(ManagedInputState::ExplicitEnded,
                          std::memory_order_release);
        if (pendingControl_ != nullptr) {
            pendingControl_->cancel();
        }
        cancelled = retireCurrentLocked({});
        pendingControl_.reset();
        attachmentCount_ = 0;
        stopping_ = false;
    }
    waitUntilInactive(cancelled);
}

void EffectRuntime::cancel() noexcept {
    std::shared_ptr<ProcessorGeneration> cancelled;
    {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        stopping_ = true;
        explicitlyStopped_.store(true, std::memory_order_release);
        inputState_.store(ManagedInputState::ExplicitEnded,
                          std::memory_order_release);
        cancelled = processorOwner_;
        if (cancelled != nullptr) {
            cancelled->cancel();
        }
    }
    waitUntilInactive(cancelled);
}

void EffectRuntime::clear() noexcept {
    std::shared_ptr<ProcessorGeneration> cancelled;
    {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        cancelled = retireCurrentLocked({});
        stopping_ = false;
    }
    waitUntilInactive(cancelled);
}

void EffectRuntime::preparePlaying() noexcept {
    {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        inputState_.store(ManagedInputState::Starting,
                          std::memory_order_release);
    }
    if (!isCallbackThread()) {
        waitForCallbacks();
    }
}

void EffectRuntime::markPlaying() noexcept {
    const std::lock_guard<std::mutex> lock(attachmentMutex_);
    if (processorOwner_ != nullptr) {
        processorOwner_->endOfStreamComplete.store(false,
                                                   std::memory_order_release);
    }
    stopping_ = false;
    explicitlyStopped_.store(false, std::memory_order_release);
    naturalStopObservedByManager_ = false;
    inputState_.store(ManagedInputState::Playing, std::memory_order_release);
}

void EffectRuntime::notifyNaturalInputEnded() noexcept {
    const std::lock_guard<std::mutex> lock(attachmentMutex_);
    if (explicitlyStopped_.load(std::memory_order_acquire) ||
        inputState_.load(std::memory_order_acquire) !=
            ManagedInputState::Playing) {
        return;
    }
    if (!naturalStopObservedByManager_) {
        naturalStopObservedByManager_ = true;
        return;
    }
    if (processorOwner_ != nullptr) {
        if (processorOwner_->requiresEndOfStreamAcknowledgement) {
            processorOwner_->control->beginTail();
        }
        processorOwner_->endOfStreamComplete.store(false,
                                                   std::memory_order_release);
    }
    ManagedInputState expected = ManagedInputState::Playing;
    inputState_.compare_exchange_strong(
        expected, ManagedInputState::NaturalEnded, std::memory_order_acq_rel,
        std::memory_order_acquire);
}

bool EffectRuntime::isNaturalInputDrained() const noexcept {
    const std::lock_guard<std::mutex> lock(attachmentMutex_);
    const ManagedInputState inputState =
        inputState_.load(std::memory_order_acquire);
    if (inputState == ManagedInputState::ExplicitEnded) {
        return true;
    }
    if (inputState != ManagedInputState::NaturalEnded) {
        return false;
    }
    return processorOwner_ == nullptr ||
           processorOwner_->endOfStreamComplete.load(std::memory_order_acquire);
}

RetiredProcessorGenerations EffectRuntime::takeAllRetired() noexcept {
    const std::lock_guard<std::mutex> lock(attachmentMutex_);
    return RetiredProcessorGenerations(std::move(retired_));
}

void EffectRuntime::waitForCallbacks() const noexcept {
    std::size_t count = callbacksInFlight_.load(std::memory_order_acquire);
    while (count != 0) {
        callbacksInFlight_.wait(count, std::memory_order_acquire);
        count = callbacksInFlight_.load(std::memory_order_acquire);
    }
}

bool EffectRuntime::wasExplicitlyStopped() const noexcept {
    return explicitlyStopped_.load(std::memory_order_acquire);
}

void EffectRuntime::completeNaturalInput(
    ProcessorGeneration* generation) noexcept {
    if (generation != nullptr) {
        if (generation->requiresEndOfStreamAcknowledgement) {
            generation->control->beginTail();
        }
        generation->endOfStreamComplete.store(false, std::memory_order_release);
    }
    ManagedInputState expected = ManagedInputState::NaturalEnding;
    inputState_.compare_exchange_strong(
        expected, ManagedInputState::NaturalEnded, std::memory_order_acq_rel,
        std::memory_order_acquire);
}

void EffectRuntime::waitUntilInactive(
    const std::shared_ptr<ProcessorGeneration>& generation) noexcept {
    if (generation != nullptr && !isCallbackThread()) {
        generation->waitUntilInactive();
    }
}

std::shared_ptr<ProcessorGeneration> EffectRuntime::retireCurrentLocked(
    std::shared_ptr<ProcessorGeneration> replacement) noexcept {
    std::shared_ptr<ProcessorGeneration> removed = std::move(processorOwner_);
    if (removed != nullptr) {
        removed->cancel();
    }
    processorOwner_ = std::move(replacement);
    processor_.store(processorOwner_.get(), std::memory_order_release);
    if (removed != nullptr) {
        removed->retiredNext = std::move(retired_);
        retired_ = removed;
    }
    return removed;
}

}  // namespace ludork::global::managed_audio_source_impl
