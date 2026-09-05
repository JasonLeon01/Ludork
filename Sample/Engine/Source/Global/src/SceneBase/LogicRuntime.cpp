#include "LogicRuntime.hpp"

#include <algorithm>

namespace ludork::global::scene_base_impl {

double durationMilliseconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

float fixedStepForFrameRate(int frameRate) {
    return 1.0f / static_cast<float>(std::max(1, frameRate));
}

float nonNegativeScaledDelta(std::chrono::steady_clock::time_point current,
                             std::chrono::steady_clock::time_point previous,
                             float speed) {
    return std::max(0.0f,
                    std::chrono::duration<float>(current - previous).count()) *
           speed;
}

}  // namespace ludork::global::scene_base_impl
