#include <Manager/TimerEntry.hpp>

#include <utility>

TimerEntry::TimerEntry(float timeValue, RuntimeIdentityPtr taskValue,
                       RuntimeValue::Array paramsValue, bool blockingValue)
    : time(timeValue),
      task(std::move(taskValue)),
      params(std::move(paramsValue)),
      blocking(blockingValue) {}

bool TimerEntry::isReady() const {
    return time <= 0.0f || isCancelled();
}

bool TimerEntry::isCancelled() const {
    return cancelled_ ||
           (operation != nullptr && operation->getStatus() == "cancelled");
}

void TimerEntry::cancel() {
    if (operation != nullptr) {
        operation->cancel();
    }
    task.reset();
    params.clear();
    cancelled_ = true;
    time = 0.0f;
}
