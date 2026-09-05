#include "AudioCallbackRuntime.hpp"

#include <algorithm>
#include <limits>

namespace ludork::global::managed_audio_source_impl {
namespace {

thread_local std::size_t callbackDepth = 0;

bool validBlock(const float* inputFrames, unsigned int& inputFrameCount,
                float* outputFrames, unsigned int& outputFrameCount,
                unsigned int frameChannelCount,
                std::size_t& sampleCount) noexcept {
    if (inputFrames == nullptr || outputFrames == nullptr ||
        frameChannelCount == 0) {
        inputFrameCount = 0;
        outputFrameCount = 0;
        return false;
    }
    const unsigned int frameCount = std::min(inputFrameCount, outputFrameCount);
    if (frameCount >
        std::numeric_limits<std::size_t>::max() / frameChannelCount) {
        inputFrameCount = 0;
        outputFrameCount = 0;
        return false;
    }
    sampleCount = static_cast<std::size_t>(frameCount) * frameChannelCount;
    inputFrameCount = frameCount;
    outputFrameCount = frameCount;
    return true;
}

}  // namespace

void enterCallback() noexcept {
    ++callbackDepth;
}

void leaveCallback() noexcept {
    --callbackDepth;
}

bool isCallbackThread() noexcept {
    return callbackDepth != 0;
}

void dryBypass(const float* inputFrames, unsigned int& inputFrameCount,
               float* outputFrames, unsigned int& outputFrameCount,
               unsigned int frameChannelCount) noexcept {
    std::size_t sampleCount = 0;
    if (!validBlock(inputFrames, inputFrameCount, outputFrames,
                    outputFrameCount, frameChannelCount, sampleCount)) {
        return;
    }
    std::copy_n(inputFrames, sampleCount, outputFrames);
}

void silenceCancelledBlock(const float* inputFrames,
                           unsigned int& inputFrameCount, float* outputFrames,
                           unsigned int& outputFrameCount,
                           unsigned int frameChannelCount) noexcept {
    std::size_t sampleCount = 0;
    if (!validBlock(inputFrames, inputFrameCount, outputFrames,
                    outputFrameCount, frameChannelCount, sampleCount)) {
        return;
    }
    std::fill_n(outputFrames, sampleCount, 0.0f);
}

void returnNoInput(unsigned int& inputFrameCount,
                   unsigned int& outputFrameCount) noexcept {
    inputFrameCount = 0;
    outputFrameCount = 0;
}

}  // namespace ludork::global::managed_audio_source_impl
