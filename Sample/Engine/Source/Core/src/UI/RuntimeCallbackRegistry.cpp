#include <UI/RuntimeCallbackRegistry.hpp>
#include <UI/ControlBase.hpp>

void RuntimeCallbackRegistry::registerControl(ControlBase* control) {
    if (control == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    controls_.insert(control);
}

void RuntimeCallbackRegistry::unregisterControl(ControlBase* control) noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    controls_.erase(control);
}

void RuntimeCallbackRegistry::releaseRuntimeCallbacks() noexcept {
    std::vector<ControlBase*> controls;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        controls.assign(controls_.begin(), controls_.end());
    }
    for (ControlBase* control : controls) {
        if (!contains(control)) {
            continue;
        }
        const std::shared_ptr<ControlBase> owner =
            control->weak_from_this().lock();
        if (owner != nullptr && owner->getParent() == nullptr) {
            owner->releaseRuntimeCallbacks();
        }
    }
}

bool RuntimeCallbackRegistry::contains(ControlBase* control) const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return controls_.contains(control);
}
