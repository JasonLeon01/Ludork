#include <Manager/AudioEffects.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using ludork::global::audio::AudioEffectBinding;
using ludork::global::audio::AudioEffectControl;
using ludork::global::audio::AudioEffectFault;

constexpr double EchoDelay = 0.3;
constexpr double EchoDecay = 0.5;
constexpr double DistortionDrive = 2.0;
constexpr double DistortionThreshold = 0.7;
constexpr double UnderwaterDepth = 0.7;
constexpr double UnderwaterBubbleIntensity = 0.3;
constexpr double BehindWallCutoff = 900.0;
constexpr double BehindWallTransmission = 0.35;
constexpr double Pi = std::numbers::pi_v<double>;
constexpr unsigned int MaximumChannelCount = 8;

using Factory = AudioEffectBinding (*)(std::uint32_t);

struct RegistryEntry {
    std::string_view name;
    Factory factory;
};

void clearFrameCounts(unsigned int& inputFrameCount,
                      unsigned int& outputFrameCount) noexcept {
    inputFrameCount = 0;
    outputFrameCount = 0;
}

bool getSampleCount(unsigned int frameCount, unsigned int channelCount,
                    std::size_t& sampleCount) noexcept {
    if (channelCount == 0 ||
        frameCount > std::numeric_limits<std::size_t>::max() / channelCount) {
        return false;
    }
    sampleCount = static_cast<std::size_t>(frameCount) * channelCount;
    return true;
}

void markFault(const std::shared_ptr<AudioEffectControl>& control,
               AudioEffectFault fault) noexcept {
    AudioEffectFault expected = AudioEffectFault::None;
    control->fault.compare_exchange_strong(expected, fault,
                                           std::memory_order_release,
                                           std::memory_order_relaxed);
    control->tailDrained.store(true, std::memory_order_release);
}

void silenceBlock(const float* inputFrames, unsigned int& inputFrameCount,
                  float* outputFrames, unsigned int& outputFrameCount,
                  unsigned int frameChannelCount,
                  const std::shared_ptr<AudioEffectControl>& control) noexcept {
    if (inputFrames == nullptr || frameChannelCount == 0) {
        clearFrameCounts(inputFrameCount, outputFrameCount);
        control->tailDrained.store(true, std::memory_order_release);
        return;
    }
    const unsigned int frameCount =
        std::min(inputFrameCount, outputFrameCount);
    std::size_t sampleCount = 0;
    if (frameCount == 0 || outputFrames == nullptr ||
        !getSampleCount(frameCount, frameChannelCount, sampleCount)) {
        clearFrameCounts(inputFrameCount, outputFrameCount);
        control->tailDrained.store(true, std::memory_order_release);
        return;
    }
    std::fill_n(outputFrames, sampleCount, 0.0f);
    inputFrameCount = frameCount;
    outputFrameCount = frameCount;
    control->tailDrained.store(true, std::memory_order_release);
}

void faultAndSilence(
    const float* inputFrames, unsigned int& inputFrameCount,
    float* outputFrames, unsigned int& outputFrameCount,
    unsigned int frameChannelCount,
    const std::shared_ptr<AudioEffectControl>& control,
    AudioEffectFault fault) noexcept {
    silenceBlock(inputFrames, inputFrameCount, outputFrames, outputFrameCount,
                 frameChannelCount, control);
    markFault(control, fault);
}

bool shouldSilence(
    const std::shared_ptr<AudioEffectControl>& control) noexcept {
    return control->cancelled.load(std::memory_order_acquire) ||
           control->fault.load(std::memory_order_acquire) !=
               AudioEffectFault::None;
}

class EchoProcessor {
public:
    EchoProcessor(std::uint32_t sampleRate,
                  std::shared_ptr<AudioEffectControl> control)
        : delayFrames_(std::max(
              1U, static_cast<unsigned int>(std::floor(
                      std::max(0.0, EchoDelay) * sampleRate)))),
          control_(std::move(control)) {
        std::size_t sampleCount = 0;
        if (!getSampleCount(delayFrames_, MaximumChannelCount, sampleCount)) {
            throw std::length_error("Echo delay buffer is too large");
        }
        delayBuffer_.assign(sampleCount, 0.0);
    }

    void operator()(const float* inputFrames, unsigned int& inputFrameCount,
                    float* outputFrames, unsigned int& outputFrameCount,
                    unsigned int frameChannelCount) noexcept {
        const unsigned int availableInput = inputFrameCount;
        const unsigned int outputCapacity = outputFrameCount;
        if (frameChannelCount == 0 ||
            frameChannelCount > MaximumChannelCount) {
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::UnsupportedChannelCount);
            return;
        }
        if (shouldSilence(control_)) {
            silenceBlock(inputFrames, inputFrameCount, outputFrames,
                         outputFrameCount, frameChannelCount, control_);
            return;
        }
        try {
            process(inputFrames, inputFrameCount, outputFrames,
                    outputFrameCount, frameChannelCount);
        } catch (...) {
            inputFrameCount = availableInput;
            outputFrameCount = outputCapacity;
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::ProcessingFailure);
        }
    }

private:
    void reset(unsigned int frameChannelCount) {
        if (bufferChannelCount_ != 0) {
            std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0);
        }
        bufferIndex_ = 0;
        bufferChannelCount_ = frameChannelCount;
        tailFrameCount_ = 0;
        control_->tailDrained.store(true, std::memory_order_release);
    }

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) {
        if (inputFrames == nullptr) {
            inputFrameCount = 0;
            if (bufferChannelCount_ == 0 ||
                bufferChannelCount_ != frameChannelCount ||
                tailFrameCount_ == 0) {
                outputFrameCount = 0;
                control_->tailDrained.store(true, std::memory_order_release);
                return;
            }
            const unsigned int frameCount =
                std::min(tailFrameCount_, outputFrameCount);
            if (frameCount > 0 && outputFrames == nullptr) {
                markFault(control_, AudioEffectFault::InvalidBuffer);
                outputFrameCount = 0;
                return;
            }
            for (unsigned int frame = 0; frame < frameCount; ++frame) {
                const std::size_t frameOffset =
                    static_cast<std::size_t>(frame) * frameChannelCount;
                const std::size_t delayOffset =
                    bufferIndex_ * MaximumChannelCount;
                for (unsigned int channel = 0; channel < frameChannelCount;
                     ++channel) {
                    const std::size_t outputIndex = frameOffset + channel;
                    const std::size_t delayIndex = delayOffset + channel;
                    outputFrames[outputIndex] = static_cast<float>(
                        delayBuffer_[delayIndex] * EchoDecay);
                    delayBuffer_[delayIndex] = 0.0f;
                }
                bufferIndex_ = (bufferIndex_ + 1) % delayFrames_;
            }
            outputFrameCount = frameCount;
            tailFrameCount_ -= frameCount;
            control_->tailDrained.store(tailFrameCount_ == 0,
                                        std::memory_order_release);
            if (control_->cancelled.load(std::memory_order_acquire)) {
                if (outputFrames != nullptr) {
                    std::size_t sampleCount = 0;
                    if (getSampleCount(frameCount, frameChannelCount,
                                       sampleCount)) {
                        std::fill_n(outputFrames, sampleCount, 0.0f);
                    }
                }
                outputFrameCount = 0;
                control_->tailDrained.store(true, std::memory_order_release);
            }
            return;
        }

        if (bufferChannelCount_ != frameChannelCount) {
            reset(frameChannelCount);
        }
        const unsigned int frameCount =
            std::min(inputFrameCount, outputFrameCount);
        std::size_t sampleCount = 0;
        if (!getSampleCount(frameCount, frameChannelCount, sampleCount)) {
            markFault(control_, AudioEffectFault::SizeOverflow);
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        if (frameCount > 0 && outputFrames == nullptr) {
            markFault(control_, AudioEffectFault::InvalidBuffer);
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        for (unsigned int frame = 0; frame < frameCount; ++frame) {
            const std::size_t frameOffset =
                static_cast<std::size_t>(frame) * frameChannelCount;
            const std::size_t delayOffset =
                bufferIndex_ * MaximumChannelCount;
            for (unsigned int channel = 0; channel < frameChannelCount;
                 ++channel) {
                const std::size_t sampleIndex = frameOffset + channel;
                const std::size_t delayIndex = delayOffset + channel;
                const float input = inputFrames[sampleIndex];
                const double delayed = delayBuffer_[delayIndex];
                outputFrames[sampleIndex] = static_cast<float>(
                    input + delayed * EchoDecay);
                delayBuffer_[delayIndex] = input;
            }
            bufferIndex_ = (bufferIndex_ + 1) % delayFrames_;
        }
        if (control_->cancelled.load(std::memory_order_acquire)) {
            std::fill_n(outputFrames, sampleCount, 0.0f);
            control_->tailDrained.store(true, std::memory_order_release);
        } else if (frameCount > 0) {
            tailFrameCount_ = delayFrames_;
            control_->tailDrained.store(false, std::memory_order_release);
        }
        inputFrameCount = frameCount;
        outputFrameCount = frameCount;
    }

    unsigned int delayFrames_;
    std::vector<double> delayBuffer_;
    std::size_t bufferIndex_ = 0;
    unsigned int bufferChannelCount_ = 0;
    unsigned int tailFrameCount_ = 0;
    std::shared_ptr<AudioEffectControl> control_;
};

class DistortionProcessor {
public:
    explicit DistortionProcessor(std::shared_ptr<AudioEffectControl> control)
        : control_(std::move(control)) {}

    void operator()(const float* inputFrames, unsigned int& inputFrameCount,
                    float* outputFrames, unsigned int& outputFrameCount,
                    unsigned int frameChannelCount) noexcept {
        if (inputFrames == nullptr) {
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        if (frameChannelCount == 0) {
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::UnsupportedChannelCount);
            return;
        }
        if (shouldSilence(control_)) {
            silenceBlock(inputFrames, inputFrameCount, outputFrames,
                         outputFrameCount, frameChannelCount, control_);
            return;
        }
        const unsigned int frameCount =
            std::min(inputFrameCount, outputFrameCount);
        std::size_t sampleCount = 0;
        if (!getSampleCount(frameCount, frameChannelCount, sampleCount)) {
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::SizeOverflow);
            return;
        }
        if (frameCount > 0 && outputFrames == nullptr) {
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::InvalidBuffer);
            return;
        }
        for (std::size_t index = 0; index < sampleCount; ++index) {
            outputFrames[index] =
                static_cast<float>(std::clamp(
                    inputFrames[index] * DistortionDrive,
                    -DistortionThreshold, DistortionThreshold));
        }
        if (control_->cancelled.load(std::memory_order_acquire)) {
            std::fill_n(outputFrames, sampleCount, 0.0f);
        }
        inputFrameCount = frameCount;
        outputFrameCount = frameCount;
    }

private:
    std::shared_ptr<AudioEffectControl> control_;
};

class UnderwaterProcessor {
public:
    UnderwaterProcessor(std::uint32_t sampleRate,
                        std::shared_ptr<AudioEffectControl> control)
        : sampleRate_(sampleRate),
          timeStep_(1.0 / sampleRate),
          bubbleRate_(0.001 + UnderwaterBubbleIntensity * 0.005),
          compressionRatio_(1.0 + UnderwaterDepth * 2.0),
          control_(std::move(control)) {
        const double cutoff =
            std::max(1.0, 800.0 - UnderwaterDepth * 600.0);
        alpha_ = timeStep_ /
                 (1.0 / (cutoff * 2.0 * Pi) + timeStep_);
        constexpr std::array<unsigned int, 5> milliseconds{
            43, 67, 89, 127, 173};
        for (std::size_t index = 0; index < milliseconds.size(); ++index) {
            delayFrames_[index] = std::max(
                1U, static_cast<unsigned int>(std::floor(
                        static_cast<double>(sampleRate_) *
                        milliseconds[index] / 1000.0)));
            std::size_t sampleCount = 0;
            if (!getSampleCount(delayFrames_[index], MaximumChannelCount,
                                sampleCount)) {
                throw std::length_error(
                    "Underwater delay buffer is too large");
            }
            buffers_[index].assign(sampleCount, 0.0);
        }
    }

    void operator()(const float* inputFrames, unsigned int& inputFrameCount,
                    float* outputFrames, unsigned int& outputFrameCount,
                    unsigned int frameChannelCount) noexcept {
        const unsigned int availableInput = inputFrameCount;
        const unsigned int outputCapacity = outputFrameCount;
        if (inputFrames == nullptr) {
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        if (frameChannelCount == 0 ||
            frameChannelCount > MaximumChannelCount) {
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::UnsupportedChannelCount);
            return;
        }
        if (shouldSilence(control_)) {
            silenceBlock(inputFrames, inputFrameCount, outputFrames,
                         outputFrameCount, frameChannelCount, control_);
            return;
        }
        try {
            process(inputFrames, inputFrameCount, outputFrames,
                    outputFrameCount, frameChannelCount);
        } catch (...) {
            inputFrameCount = availableInput;
            outputFrameCount = outputCapacity;
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::ProcessingFailure);
        }
    }

private:
    double nextRandom() noexcept {
        randomState_ = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(randomState_) * 48271U) %
            2147483647U);
        return static_cast<double>(randomState_) / 2147483647.0;
    }

    void reset(unsigned int frameChannelCount) {
        if (bufferChannelCount_ != 0) {
            for (std::vector<double>& buffer : buffers_) {
                std::fill(buffer.begin(), buffer.end(), 0.0);
            }
        }
        previous_.fill(0.0);
        indices_.fill(0);
        bufferChannelCount_ = frameChannelCount;
        envelope_ = 0.0;
    }

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) {
        if (bufferChannelCount_ != frameChannelCount) {
            reset(frameChannelCount);
        }
        const unsigned int frameCount =
            std::min(inputFrameCount, outputFrameCount);
        std::size_t sampleCount = 0;
        if (!getSampleCount(frameCount, frameChannelCount, sampleCount)) {
            markFault(control_, AudioEffectFault::SizeOverflow);
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        if (frameCount > 0 && outputFrames == nullptr) {
            markFault(control_, AudioEffectFault::InvalidBuffer);
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        for (unsigned int frame = 0; frame < frameCount; ++frame) {
            const double modulation =
                1.0 + 0.05 * UnderwaterDepth * std::sin(waterPhase_);
            waterPhase_ = std::fmod(
                waterPhase_ + 2.0 * Pi * 0.5 / sampleRate_, 2.0 * Pi);
            double bubble = 0.0;
            if (nextRandom() < bubbleRate_) {
                const double frequency = 200.0 + nextRandom() * 800.0;
                const double amplitude =
                    (nextRandom() * 0.5 + 0.5) *
                    UnderwaterBubbleIntensity * 0.1;
                bubble = amplitude * std::sin(bubblePhase_ * frequency) *
                         std::exp(-bubblePhase_ * 10.0);
            }
            bubblePhase_ = std::fmod(bubblePhase_ + timeStep_, 1.0);
            const std::size_t frameOffset =
                static_cast<std::size_t>(frame) * frameChannelCount;
            for (unsigned int channel = 0; channel < frameChannelCount;
                 ++channel) {
                const std::size_t sampleIndex = frameOffset + channel;
                double signal = inputFrames[sampleIndex] * modulation;
                const double absolute = std::abs(signal);
                const double smoothing = absolute > envelope_ ? 0.95 : 0.999;
                envelope_ = absolute + (envelope_ - absolute) * smoothing;
                if (envelope_ > 0.3) {
                    const double gain =
                        0.3 + (envelope_ - 0.3) / compressionRatio_;
                    signal = signal * gain / envelope_;
                }
                const double filtered =
                    alpha_ * signal + (1.0 - alpha_) * previous_[channel];
                previous_[channel] = filtered;
                double reverb = 0.0;
                for (std::size_t index = 0; index < buffers_.size(); ++index) {
                    const std::size_t offset =
                        indices_[index] * MaximumChannelCount + channel;
                    const double delayed = buffers_[index][offset];
                    reverb += delayed * (0.2 + UnderwaterDepth * 0.3);
                    buffers_[index][offset] =
                        filtered + delayed * (0.7 - UnderwaterDepth * 0.2);
                }
                double channelBubble = bubble;
                if (channel != 0) {
                    channelBubble = bubble * (0.7 + nextRandom() * 0.6);
                }
                const double output =
                    (filtered + reverb * 0.4 + channelBubble) *
                    (1.0 - UnderwaterDepth * 0.4);
                outputFrames[sampleIndex] = static_cast<float>(
                    std::clamp(output, -0.8, 0.8));
            }
            for (std::size_t index = 0; index < indices_.size(); ++index) {
                indices_[index] =
                    (indices_[index] + 1) % delayFrames_[index];
            }
        }
        if (control_->cancelled.load(std::memory_order_acquire)) {
            std::fill_n(outputFrames, sampleCount, 0.0f);
        }
        inputFrameCount = frameCount;
        outputFrameCount = frameCount;
    }

    std::uint32_t sampleRate_;
    double timeStep_;
    double alpha_ = 0.0;
    double bubbleRate_;
    double compressionRatio_;
    std::array<unsigned int, 5> delayFrames_{};
    std::array<std::vector<double>, 5> buffers_;
    std::array<std::size_t, 5> indices_{};
    std::array<double, MaximumChannelCount> previous_{};
    unsigned int bufferChannelCount_ = 0;
    double bubblePhase_ = 0.0;
    double waterPhase_ = 0.0;
    double envelope_ = 0.0;
    std::uint32_t randomState_ = 104729;
    std::shared_ptr<AudioEffectControl> control_;
};

class BehindWallProcessor {
public:
    BehindWallProcessor(std::uint32_t sampleRate,
                        std::shared_ptr<AudioEffectControl> control)
        : control_(std::move(control)) {
        const double maximumCutoff =
            std::max(20.0, static_cast<double>(sampleRate) * 0.45);
        const double cutoff = std::clamp(
            static_cast<double>(BehindWallCutoff), 20.0, maximumCutoff);
        alpha_ = 1.0 - std::exp(-2.0 * Pi * cutoff / sampleRate);
        gain_ = std::clamp(BehindWallTransmission, 0.0, 1.0);
    }

    void operator()(const float* inputFrames, unsigned int& inputFrameCount,
                    float* outputFrames, unsigned int& outputFrameCount,
                    unsigned int frameChannelCount) noexcept {
        const unsigned int availableInput = inputFrameCount;
        const unsigned int outputCapacity = outputFrameCount;
        if (inputFrames == nullptr) {
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        if (frameChannelCount == 0 ||
            frameChannelCount > MaximumChannelCount) {
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::UnsupportedChannelCount);
            return;
        }
        if (shouldSilence(control_)) {
            silenceBlock(inputFrames, inputFrameCount, outputFrames,
                         outputFrameCount, frameChannelCount, control_);
            return;
        }
        try {
            process(inputFrames, inputFrameCount, outputFrames,
                    outputFrameCount, frameChannelCount);
        } catch (...) {
            inputFrameCount = availableInput;
            outputFrameCount = outputCapacity;
            faultAndSilence(inputFrames, inputFrameCount, outputFrames,
                            outputFrameCount, frameChannelCount, control_,
                            AudioEffectFault::ProcessingFailure);
        }
    }

private:
    void reset(unsigned int frameChannelCount) {
        if (filterChannelCount_ != 0) {
            firstStages_.fill(0.0);
            secondStages_.fill(0.0);
        }
        filterChannelCount_ = frameChannelCount;
    }

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) {
        if (filterChannelCount_ != frameChannelCount) {
            reset(frameChannelCount);
        }
        const unsigned int frameCount =
            std::min(inputFrameCount, outputFrameCount);
        std::size_t sampleCount = 0;
        if (!getSampleCount(frameCount, frameChannelCount, sampleCount)) {
            markFault(control_, AudioEffectFault::SizeOverflow);
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        if (frameCount > 0 && outputFrames == nullptr) {
            markFault(control_, AudioEffectFault::InvalidBuffer);
            clearFrameCounts(inputFrameCount, outputFrameCount);
            return;
        }
        for (unsigned int frame = 0; frame < frameCount; ++frame) {
            const std::size_t frameOffset =
                static_cast<std::size_t>(frame) * frameChannelCount;
            for (unsigned int channel = 0; channel < frameChannelCount;
                 ++channel) {
                const std::size_t sampleIndex = frameOffset + channel;
                const double input = inputFrames[sampleIndex];
                const double first =
                    firstStages_[channel] +
                    alpha_ * (input - firstStages_[channel]);
                const double second =
                    secondStages_[channel] +
                    alpha_ * (first - secondStages_[channel]);
                firstStages_[channel] = first;
                secondStages_[channel] = second;
                outputFrames[sampleIndex] = static_cast<float>(
                    std::clamp(second * gain_, -1.0, 1.0));
            }
        }
        if (control_->cancelled.load(std::memory_order_acquire)) {
            std::fill_n(outputFrames, sampleCount, 0.0f);
        }
        inputFrameCount = frameCount;
        outputFrameCount = frameCount;
    }

    double alpha_ = 0.0;
    double gain_ = 0.0;
    std::array<double, MaximumChannelCount> firstStages_{};
    std::array<double, MaximumChannelCount> secondStages_{};
    unsigned int filterChannelCount_ = 0;
    std::shared_ptr<AudioEffectControl> control_;
};

AudioEffectBinding createEcho(std::uint32_t sampleRate) {
    auto control = std::make_shared<AudioEffectControl>();
    auto state = std::make_shared<EchoProcessor>(sampleRate, control);
    return {sf::SoundSource::EffectProcessor(
                [state](const float* inputFrames,
                        unsigned int& inputFrameCount, float* outputFrames,
                        unsigned int& outputFrameCount,
                        unsigned int frameChannelCount) noexcept {
                    (*state)(inputFrames, inputFrameCount, outputFrames,
                             outputFrameCount, frameChannelCount);
                }),
            std::move(control)};
}

AudioEffectBinding createDistortion(std::uint32_t) {
    auto control = std::make_shared<AudioEffectControl>();
    auto state = std::make_shared<DistortionProcessor>(control);
    return {sf::SoundSource::EffectProcessor(
                [state](const float* inputFrames,
                        unsigned int& inputFrameCount, float* outputFrames,
                        unsigned int& outputFrameCount,
                        unsigned int frameChannelCount) noexcept {
                    (*state)(inputFrames, inputFrameCount, outputFrames,
                             outputFrameCount, frameChannelCount);
                }),
            std::move(control)};
}

AudioEffectBinding createUnderwater(std::uint32_t sampleRate) {
    auto control = std::make_shared<AudioEffectControl>();
    auto state = std::make_shared<UnderwaterProcessor>(sampleRate, control);
    return {sf::SoundSource::EffectProcessor(
                [state](const float* inputFrames,
                        unsigned int& inputFrameCount, float* outputFrames,
                        unsigned int& outputFrameCount,
                        unsigned int frameChannelCount) noexcept {
                    (*state)(inputFrames, inputFrameCount, outputFrames,
                             outputFrameCount, frameChannelCount);
                }),
            std::move(control)};
}

AudioEffectBinding createBehindWall(std::uint32_t sampleRate) {
    auto control = std::make_shared<AudioEffectControl>();
    auto state = std::make_shared<BehindWallProcessor>(sampleRate, control);
    return {sf::SoundSource::EffectProcessor(
                [state](const float* inputFrames,
                        unsigned int& inputFrameCount, float* outputFrames,
                        unsigned int& outputFrameCount,
                        unsigned int frameChannelCount) noexcept {
                    (*state)(inputFrames, inputFrameCount, outputFrames,
                             outputFrameCount, frameChannelCount);
                }),
            std::move(control)};
}

constexpr std::array<RegistryEntry, 5> Registry{{
    {"nil", nullptr},
    {"Echo", createEcho},
    {"Distortion", createDistortion},
    {"Underwater", createUnderwater},
    {"BehindWall", createBehindWall},
}};

const RegistryEntry* findEntry(std::string_view name) noexcept {
    const auto iterator =
        std::find_if(Registry.begin(), Registry.end(),
                     [name](const RegistryEntry& entry) {
                         return entry.name == name;
                     });
    return iterator == Registry.end() ? nullptr : &*iterator;
}

std::unordered_map<std::string, std::string> makeAudioEffectTokens() {
    std::unordered_map<std::string, std::string> result;
    result.reserve(Registry.size());
    for (const RegistryEntry& entry : Registry) {
        result.emplace(entry.name, entry.name);
    }
    return result;
}

}  // namespace

const std::unordered_map<std::string, std::string> AudioEffects =
    makeAudioEffectTokens();

namespace ludork::global::audio {

void validateAudioEffect(std::string_view name) {
    const RegistryEntry* entry = findEntry(name);
    if (entry == nullptr) {
        throw std::invalid_argument("Unknown audio effect: " +
                                    std::string(name));
    }
}

bool hasAudioEffectProcessor(std::string_view name) {
    const RegistryEntry* entry = findEntry(name);
    if (entry == nullptr) {
        throw std::invalid_argument("Unknown audio effect: " +
                                    std::string(name));
    }
    return entry->factory != nullptr;
}

AudioEffectBinding createAudioEffect(std::string_view name,
                                     std::uint32_t sampleRate) {
    const RegistryEntry* entry = findEntry(name);
    if (entry == nullptr) {
        throw std::invalid_argument("Unknown audio effect: " +
                                    std::string(name));
    }
    if (entry->factory == nullptr) {
        return {};
    }
    if (sampleRate == 0) {
        throw std::invalid_argument(
            "Audio playback device sample rate must be positive");
    }
    return entry->factory(sampleRate);
}

void cancelAudioEffect(
    const std::shared_ptr<AudioEffectControl>& control) noexcept {
    if (control == nullptr) {
        return;
    }
    control->cancelled.store(true, std::memory_order_release);
    control->tailDrained.store(true, std::memory_order_release);
}

bool isAudioEffectDrained(
    const std::shared_ptr<AudioEffectControl>& control) noexcept {
    return control == nullptr ||
           control->cancelled.load(std::memory_order_acquire) ||
           control->tailDrained.load(std::memory_order_acquire);
}

AudioEffectFault takeAudioEffectFault(
    const std::shared_ptr<AudioEffectControl>& control) noexcept {
    if (control == nullptr) {
        return AudioEffectFault::None;
    }
    const AudioEffectFault fault =
        control->fault.load(std::memory_order_acquire);
    if (fault == AudioEffectFault::None) {
        return AudioEffectFault::None;
    }
    bool expected = false;
    return control->faultReported.compare_exchange_strong(
               expected, true, std::memory_order_acq_rel,
               std::memory_order_relaxed)
               ? fault
               : AudioEffectFault::None;
}

const char* audioEffectFaultMessage(AudioEffectFault fault) noexcept {
    switch (fault) {
        case AudioEffectFault::None:
            return "No audio effect fault";
        case AudioEffectFault::InvalidBuffer:
            return "Native audio effect received an invalid callback buffer";
        case AudioEffectFault::SizeOverflow:
            return "Native audio effect callback size overflowed";
        case AudioEffectFault::UnsupportedChannelCount:
            return "Native audio effect received an unsupported channel count";
        case AudioEffectFault::ProcessingFailure:
            return "Native audio effect processing failed";
    }
    return "Unknown native audio effect fault";
}

}  // namespace ludork::global::audio
