#include <Manager/ManagedAudioSource.hpp>

#include "AudioEffectLuaRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

bool AudioEffectControl::isCancelled() const noexcept {
    return state_.load(std::memory_order_acquire) ==
           AudioEffectState::Cancelled;
}

void AudioEffectControl::beginTail() noexcept {
    AudioEffectState expected = AudioEffectState::Drained;
    state_.compare_exchange_strong(expected, AudioEffectState::TailPending,
                                   std::memory_order_acq_rel,
                                   std::memory_order_acquire);
}

void AudioEffectControl::finishTail() noexcept {
    AudioEffectState expected = AudioEffectState::TailPending;
    state_.compare_exchange_strong(expected, AudioEffectState::Drained,
                                   std::memory_order_acq_rel,
                                   std::memory_order_acquire);
}

void AudioEffectControl::attachLuaProcessor(sf::SoundSource& source,
                                            const std::string& name,
                                            std::uint32_t sampleRate) {
    source.setEffectProcessor(
        ludork::global::audio::createLuaAudioEffectProcessor(
            name, shared_from_this(), sampleRate));
}

void AudioEffectControl::cancel() noexcept {
    state_.store(AudioEffectState::Cancelled, std::memory_order_release);
}

bool AudioEffectControl::isDrained() const noexcept {
    return state_.load(std::memory_order_acquire) !=
           AudioEffectState::TailPending;
}

namespace ludork::global::audio {

namespace {

thread_local std::size_t managedAudioCallbackDepth = 0;

void requireManagedAudioLifecycleCaller() {
    if (managedAudioCallbackDepth != 0) {
        throw std::logic_error(
            "Managed audio source lifecycle cannot change from an effect "
            "processor");
    }
}

enum class ManagedInputState : std::uint8_t {
    Dormant,
    Starting,
    Playing,
    NaturalEnding,
    NaturalEnded,
    ExplicitEnded
};

const sf::SoundBuffer& requireSoundBuffer(
    const std::shared_ptr<const sf::SoundBuffer>& buffer) {
    if (buffer == nullptr) {
        throw std::invalid_argument("Managed sound buffer is required");
    }
    return *buffer;
}

void dryBypass(const float* inputFrames, unsigned int& inputFrameCount,
               float* outputFrames, unsigned int& outputFrameCount,
               unsigned int frameChannelCount) noexcept {
    if (inputFrames == nullptr || outputFrames == nullptr ||
        frameChannelCount == 0) {
        inputFrameCount = 0;
        outputFrameCount = 0;
        return;
    }
    const unsigned int frameCount = std::min(inputFrameCount, outputFrameCount);
    if (frameCount >
        std::numeric_limits<std::size_t>::max() / frameChannelCount) {
        inputFrameCount = 0;
        outputFrameCount = 0;
        return;
    }
    const std::size_t sampleCount =
        static_cast<std::size_t>(frameCount) * frameChannelCount;
    std::copy_n(inputFrames, sampleCount, outputFrames);
    inputFrameCount = frameCount;
    outputFrameCount = frameCount;
}

void silenceCancelledBlock(const float* inputFrames,
                           unsigned int& inputFrameCount, float* outputFrames,
                           unsigned int& outputFrameCount,
                           unsigned int frameChannelCount) noexcept {
    if (inputFrames == nullptr || outputFrames == nullptr ||
        frameChannelCount == 0) {
        inputFrameCount = 0;
        outputFrameCount = 0;
        return;
    }
    const unsigned int frameCount = std::min(inputFrameCount, outputFrameCount);
    if (frameCount >
        std::numeric_limits<std::size_t>::max() / frameChannelCount) {
        inputFrameCount = 0;
        outputFrameCount = 0;
        return;
    }
    const std::size_t sampleCount =
        static_cast<std::size_t>(frameCount) * frameChannelCount;
    std::fill_n(outputFrames, sampleCount, 0.0f);
    inputFrameCount = frameCount;
    outputFrameCount = frameCount;
}

void returnNoInput(unsigned int& inputFrameCount,
                   unsigned int& outputFrameCount) noexcept {
    inputFrameCount = 0;
    outputFrameCount = 0;
}

struct ProcessorGeneration {
    static constexpr std::uint32_t CancelledBit = 1U << 31U;
    static constexpr std::uint32_t ActivityMask = CancelledBit - 1U;

    std::shared_ptr<AudioEffectControl> control;
    sf::SoundSource::EffectProcessor processor;
    std::shared_ptr<ProcessorGeneration> retiredNext;
    std::atomic_uint32_t gate{0};
    std::atomic_bool endOfStreamComplete{false};
    bool requiresEndOfStreamAcknowledgement = false;

    bool tryAcquireActivity() noexcept {
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

void destroyProcessorGenerations(
    std::shared_ptr<ProcessorGeneration> generation) noexcept {
    while (generation != nullptr) {
        std::shared_ptr<ProcessorGeneration> next =
            std::move(generation->retiredNext);
        generation.reset();
        generation = std::move(next);
    }
}

}  // namespace

bool isManagedAudioCallbackThread() noexcept {
    return managedAudioCallbackDepth != 0;
}

class ManagedAudioEffectState {
public:
    explicit ManagedAudioEffectState(const sf::SoundSource& source) noexcept
        : source_(&source) {}

    sf::SoundSource::EffectProcessor makeTrampoline() {
        return [this](const float* inputFrames, unsigned int& inputFrameCount,
                      float* outputFrames, unsigned int& outputFrameCount,
                      unsigned int frameChannelCount) {
            ++managedAudioCallbackDepth;
            callbacksInFlight_.fetch_add(1, std::memory_order_acq_rel);
            struct CallbackScope {
                ManagedAudioEffectState* state;

                ~CallbackScope() {
                    if (state->callbacksInFlight_.fetch_sub(
                            1, std::memory_order_acq_rel) == 1) {
                        state->callbacksInFlight_.notify_all();
                    }
                    --managedAudioCallbackDepth;
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
            if (endOfStream && generation->endOfStreamComplete.load(
                                   std::memory_order_acquire)) {
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
                                  inputFrameCount, outputFrames,
                                  outputFrameCount, frameChannelCount);
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
                silenceCancelledBlock(inputFrames, inputFrameCount,
                                      outputFrames, outputFrameCount,
                                      frameChannelCount);
                return;
            }
            if (endOfStream && inputFrameCount == 0 && outputFrameCount == 0 &&
                (!generation->requiresEndOfStreamAcknowledgement ||
                 generation->control->isDrained())) {
                generation->control->finishTail();
                generation->endOfStreamComplete.store(
                    true, std::memory_order_release);
            }
        };
    }

    void begin(const std::shared_ptr<AudioEffectControl>& control) {
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

    void replace(sf::SoundSource::EffectProcessor effectProcessor) {
        std::shared_ptr<ProcessorGeneration> cancelled;
        {
            const std::lock_guard<std::mutex> lock(attachmentMutex_);
            if (stopping_) {
                throw std::logic_error(
                    "Audio effect processor cannot be replaced while stopping");
            }
            std::shared_ptr<AudioEffectControl> control;
            const bool requiresEndOfStreamAcknowledgement =
                pendingControl_ != nullptr;
            if (pendingControl_ != nullptr) {
                if (!effectProcessor) {
                    throw std::invalid_argument(
                        "Audio effect attacher installed an empty processor");
                }
                if (attachmentCount_ != 0) {
                    throw std::logic_error(
                        "Audio effect attacher installed more than one "
                        "processor");
                }
                control = pendingControl_;
            } else if (effectProcessor) {
                control = std::make_shared<AudioEffectControl>();
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

    void finish() {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        const std::shared_ptr<ProcessorGeneration>& generation =
            processorOwner_;
        if (pendingControl_ == nullptr || attachmentCount_ != 1 ||
            generation == nullptr || generation->control != pendingControl_) {
            throw std::logic_error(
                "Audio effect attacher did not install exactly one processor");
        }
        pendingControl_.reset();
        attachmentCount_ = 0;
    }

    void abort() noexcept {
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

    void cancel() noexcept {
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

    void clear() noexcept {
        std::shared_ptr<ProcessorGeneration> cancelled;
        {
            const std::lock_guard<std::mutex> lock(attachmentMutex_);
            cancelled = retireCurrentLocked({});
            stopping_ = false;
        }
        waitUntilInactive(cancelled);
    }

    void preparePlaying() noexcept {
        {
            const std::lock_guard<std::mutex> lock(attachmentMutex_);
            inputState_.store(ManagedInputState::Starting,
                              std::memory_order_release);
        }
        if (managedAudioCallbackDepth == 0) {
            waitForCallbacks();
        }
    }

    void markPlaying() noexcept {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        if (processorOwner_ != nullptr) {
            processorOwner_->endOfStreamComplete.store(
                false, std::memory_order_release);
        }
        stopping_ = false;
        explicitlyStopped_.store(false, std::memory_order_release);
        naturalStopObservedByManager_ = false;
        inputState_.store(ManagedInputState::Playing,
                          std::memory_order_release);
    }

    void notifyNaturalInputEnded() noexcept {
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
            processorOwner_->endOfStreamComplete.store(
                false, std::memory_order_release);
        }
        ManagedInputState expected = ManagedInputState::Playing;
        inputState_.compare_exchange_strong(
            expected, ManagedInputState::NaturalEnded,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    [[nodiscard]] bool isNaturalInputDrained() const noexcept {
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
               processorOwner_->endOfStreamComplete.load(
                   std::memory_order_acquire);
    }

    std::shared_ptr<ProcessorGeneration> takeAllRetired() noexcept {
        const std::lock_guard<std::mutex> lock(attachmentMutex_);
        return std::move(retired_);
    }

    void waitForCallbacks() const noexcept {
        std::size_t count = callbacksInFlight_.load(std::memory_order_acquire);
        while (count != 0) {
            callbacksInFlight_.wait(count, std::memory_order_acquire);
            count = callbacksInFlight_.load(std::memory_order_acquire);
        }
    }

    [[nodiscard]] bool wasExplicitlyStopped() const noexcept {
        return explicitlyStopped_.load(std::memory_order_acquire);
    }

private:
    void completeNaturalInput(ProcessorGeneration* generation) noexcept {
        if (generation != nullptr) {
            if (generation->requiresEndOfStreamAcknowledgement) {
                generation->control->beginTail();
            }
            generation->endOfStreamComplete.store(false,
                                                  std::memory_order_release);
        }
        ManagedInputState expected = ManagedInputState::NaturalEnding;
        inputState_.compare_exchange_strong(
            expected, ManagedInputState::NaturalEnded,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    static void waitUntilInactive(
        const std::shared_ptr<ProcessorGeneration>& generation) noexcept {
        if (generation != nullptr && managedAudioCallbackDepth == 0) {
            generation->waitUntilInactive();
        }
    }

    std::shared_ptr<ProcessorGeneration> retireCurrentLocked(
        std::shared_ptr<ProcessorGeneration> replacement) noexcept {
        std::shared_ptr<ProcessorGeneration> removed =
            std::move(processorOwner_);
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

    std::atomic<ProcessorGeneration*> processor_{nullptr};
    const sf::SoundSource* source_;
    std::shared_ptr<ProcessorGeneration> processorOwner_;
    std::shared_ptr<ProcessorGeneration> retired_;
    mutable std::mutex attachmentMutex_;
    std::shared_ptr<AudioEffectControl> pendingControl_;
    std::size_t attachmentCount_ = 0;
    std::atomic_bool explicitlyStopped_{false};
    std::atomic_size_t callbacksInFlight_{0};
    std::atomic<ManagedInputState> inputState_{ManagedInputState::Dormant};
    bool naturalStopObservedByManager_ = false;
    bool stopping_ = false;
};

ManagedSoundBufferOwner::ManagedSoundBufferOwner(
    const std::shared_ptr<const sf::SoundBuffer>& buffer)
    : buffer_(buffer) {
    static_cast<void>(requireSoundBuffer(buffer));
}

ManagedSound::ManagedSound(const std::shared_ptr<const sf::SoundBuffer>& buffer)
    : ManagedSoundBufferOwner(buffer),
      sf::Sound(requireSoundBuffer(buffer)),
      effectState_(std::make_unique<ManagedAudioEffectState>(*this)) {
    sf::Sound::setEffectProcessor(effectState_->makeTrampoline());
}

ManagedSound::~ManagedSound() {
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Sound::stop();
        effectState_->clear();
        sf::Sound::setEffectProcessor({});
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedSound::play() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->preparePlaying();
    sf::Sound::play();
    effectState_->markPlaying();
}

void ManagedSound::pause() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Sound::pause();
}

void ManagedSound::stop() {
    requireManagedAudioLifecycleCaller();
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Sound::stop();
        effectState_->clear();
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedSound::setEffectProcessor(EffectProcessor effectProcessor) {
    requireManagedAudioLifecycleCaller();
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->replace(std::move(effectProcessor));
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedSound::beginEffectAttachment(
    const std::shared_ptr<AudioEffectControl>& control) {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->begin(control);
}

void ManagedSound::finishEffectAttachment() {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->finish();
}

void ManagedSound::abortEffectAttachment() {
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->abort();
        if (managedAudioCallbackDepth == 0) {
            effectState_->waitForCallbacks();
            reclaimed = effectState_->takeAllRetired();
        }
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedSound::notifyNaturalInputEnded() noexcept {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    if (sf::Sound::getStatus() == sf::SoundSource::Status::Stopped &&
        !effectState_->wasExplicitlyStopped()) {
        effectState_->notifyNaturalInputEnded();
    }
}

bool ManagedSound::isNaturalInputDrained() const noexcept {
    return effectState_->isNaturalInputDrained();
}

bool ManagedSound::wasExplicitlyStopped() const noexcept {
    return effectState_->wasExplicitlyStopped();
}

ManagedMusic::ManagedMusic()
    : effectState_(std::make_unique<ManagedAudioEffectState>(*this)) {
    sf::Music::setEffectProcessor(effectState_->makeTrampoline());
}

ManagedMusic::~ManagedMusic() {
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Music::stop();
        effectState_->clear();
        sf::Music::setEffectProcessor({});
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedMusic::play() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->preparePlaying();
    sf::Music::play();
    effectState_->markPlaying();
}

void ManagedMusic::pause() {
    requireManagedAudioLifecycleCaller();
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    sf::Music::pause();
}

void ManagedMusic::stop() {
    requireManagedAudioLifecycleCaller();
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->cancel();
        sf::Music::stop();
        effectState_->clear();
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedMusic::setEffectProcessor(EffectProcessor effectProcessor) {
    requireManagedAudioLifecycleCaller();
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->replace(std::move(effectProcessor));
        effectState_->waitForCallbacks();
        reclaimed = effectState_->takeAllRetired();
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedMusic::beginEffectAttachment(
    const std::shared_ptr<AudioEffectControl>& control) {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->begin(control);
}

void ManagedMusic::finishEffectAttachment() {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    effectState_->finish();
}

void ManagedMusic::abortEffectAttachment() {
    std::shared_ptr<ProcessorGeneration> reclaimed;
    {
        const std::lock_guard<std::mutex> lock(mutationMutex_);
        effectState_->abort();
        if (managedAudioCallbackDepth == 0) {
            effectState_->waitForCallbacks();
            reclaimed = effectState_->takeAllRetired();
        }
    }
    destroyProcessorGenerations(std::move(reclaimed));
}

void ManagedMusic::notifyNaturalInputEnded() noexcept {
    const std::lock_guard<std::mutex> lock(mutationMutex_);
    if (sf::Music::getStatus() == sf::SoundSource::Status::Stopped &&
        !effectState_->wasExplicitlyStopped()) {
        effectState_->notifyNaturalInputEnded();
    }
}

bool ManagedMusic::isNaturalInputDrained() const noexcept {
    return effectState_->isNaturalInputDrained();
}

bool ManagedMusic::wasExplicitlyStopped() const noexcept {
    return effectState_->wasExplicitlyStopped();
}

}  // namespace ludork::global::audio
