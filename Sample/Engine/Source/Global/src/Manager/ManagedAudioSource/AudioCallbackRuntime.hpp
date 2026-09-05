#pragma once

#include <cstddef>

namespace ludork::global::managed_audio_source_impl {

void enterCallback() noexcept;
void leaveCallback() noexcept;
bool isCallbackThread() noexcept;
void dryBypass(const float* inputFrames, unsigned int& inputFrameCount,
               float* outputFrames, unsigned int& outputFrameCount,
               unsigned int frameChannelCount) noexcept;
void silenceCancelledBlock(const float* inputFrames,
                           unsigned int& inputFrameCount, float* outputFrames,
                           unsigned int& outputFrameCount,
                           unsigned int frameChannelCount) noexcept;
void returnNoInput(unsigned int& inputFrameCount,
                   unsigned int& outputFrameCount) noexcept;

}  // namespace ludork::global::managed_audio_source_impl
