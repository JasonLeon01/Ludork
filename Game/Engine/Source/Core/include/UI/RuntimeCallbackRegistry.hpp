#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <mutex>
#include <unordered_set>

class ControlBase;

class LUDORK_ENGINE_API RuntimeCallbackRegistry {
public:
    void registerControl(ControlBase* control);
    void unregisterControl(ControlBase* control) noexcept;
    void releaseRuntimeCallbacks() noexcept;

private:
    bool contains(ControlBase* control) const noexcept;

    mutable std::mutex mutex_;
    std::unordered_set<ControlBase*> controls_;
};
