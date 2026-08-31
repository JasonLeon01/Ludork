#include "TransitionRuntime.hpp"

#include <algorithm>

namespace ludork::global::system_transition_impl {

float advanceElapsed(float elapsed, float duration, float deltaTime) {
    return std::min(elapsed + deltaTime, duration);
}

bool isComplete(float elapsed, float duration) {
    return elapsed >= duration;
}

}  // namespace ludork::global::system_transition_impl
