#pragma once

#include <Manager/TimeManager.hpp>

#include <memory>
#include <vector>

namespace ludork::global::scene_base_impl {

std::vector<std::shared_ptr<TimerEntry>> advanceTimers(
    std::vector<std::shared_ptr<TimerEntry>>& entries, float deltaTime,
    int& blockingTimerCount);

}  // namespace ludork::global::scene_base_impl
