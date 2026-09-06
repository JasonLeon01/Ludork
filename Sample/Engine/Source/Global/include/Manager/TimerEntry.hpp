#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS()
class TimerEntry {
public:
    BIND_INIT(defaults = {nil, {}, false})
    TimerEntry(float time, RuntimeIdentityPtr task = {},
               RuntimeValue::Array params = {}, bool blocking = false);

    BIND_PROPERTY()
    float time;

    BIND_PROPERTY(metadata_type = "function")
    RuntimeIdentityPtr task;

    BIND_PROPERTY()
    RuntimeValue::Array params;

    BIND_PROPERTY()
    bool blocking;

    BIND_METHOD()
    bool isReady() const;

    BIND_METHOD(Pure = true)
    bool isCancelled() const;

    BIND_METHOD()
    void cancel();

private:
    bool cancelled_ = false;
};
