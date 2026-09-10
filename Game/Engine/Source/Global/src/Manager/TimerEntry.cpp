#include <Manager/TimerEntry.hpp>

#include <utility>

TimerEntry::TimerEntry(float timeValue, RuntimeIdentityPtr taskValue,
                       RuntimeValue::Array paramsValue, bool blockingValue)
    : time(timeValue),
      task(std::move(taskValue)),
      params(std::move(paramsValue)),
      blocking(blockingValue) {}

bool TimerEntry::isReady() const {
    return time <= 0.0f;
}

bool TimerEntry::isCancelled() const {
    return cancelled_;
}

void TimerEntry::cancel() {
    cancelled_ = true;
    time = 0.0f;
}
