#pragma once

namespace ludork::global::system_transition_impl {

float advanceElapsed(float elapsed, float duration, float deltaTime);
bool isComplete(float elapsed, float duration);

}  // namespace ludork::global::system_transition_impl
