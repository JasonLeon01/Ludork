#pragma once

#include <chrono>

namespace ludork::global::scene_base_impl {

double durationMilliseconds(std::chrono::steady_clock::duration duration);
float fixedStepForFrameRate(int frameRate);
float nonNegativeScaledDelta(std::chrono::steady_clock::time_point current,
                             std::chrono::steady_clock::time_point previous,
                             float speed);

}  // namespace ludork::global::scene_base_impl
