#include "TimerRuntime.hpp"

#include <algorithm>

namespace ludork::global::scene_base_impl {

std::vector<std::shared_ptr<TimerEntry>> advanceTimers(
    std::vector<std::shared_ptr<TimerEntry>>& entries, float deltaTime,
    int& blockingTimerCount) {
    std::vector<std::shared_ptr<TimerEntry>> ready;
    for (const std::shared_ptr<TimerEntry>& entry : entries) {
        entry->time = std::max(0.0f, entry->time - deltaTime);
        if (entry->isReady()) {
            ready.push_back(entry);
        }
    }
    for (const std::shared_ptr<TimerEntry>& entry : ready) {
        const auto iterator = std::find(entries.begin(), entries.end(), entry);
        if (iterator != entries.end()) {
            entries.erase(iterator);
        }
        if (entry != nullptr && entry->blocking) {
            entry->blocking = false;
            blockingTimerCount = std::max(0, blockingTimerCount - 1);
        }
    }
    return ready;
}

}  // namespace ludork::global::scene_base_impl
