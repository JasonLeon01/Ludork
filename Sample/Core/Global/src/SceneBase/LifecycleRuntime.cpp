#include "LifecycleRuntime.hpp"

namespace ludork::global::scene_base_impl {

bool LifecycleRuntime::tryStartMain() noexcept {
    return !destroyed_ && !mainRunning_.exchange(true);
}

void LifecycleRuntime::finishMain() noexcept {
    mainRunning_.store(false);
}

bool LifecycleRuntime::isRunning() const noexcept {
    return mainRunning_.load();
}

bool LifecycleRuntime::isCreated() const noexcept {
    return created_;
}

bool LifecycleRuntime::isEntered() const noexcept {
    return entered_;
}

bool LifecycleRuntime::isDestroyed() const noexcept {
    return destroyed_;
}

void LifecycleRuntime::markCreated() noexcept {
    created_ = true;
}

void LifecycleRuntime::markEntered() noexcept {
    entered_ = true;
    stopping_.store(false);
}

void LifecycleRuntime::markExited() noexcept {
    entered_ = false;
}

void LifecycleRuntime::markDestroyed() noexcept {
    destroyed_ = true;
}

void LifecycleRuntime::shutdown() noexcept {
    entered_ = false;
    destroyed_ = true;
}

void LifecycleRuntime::requestStop() noexcept {
    stopping_.store(true);
}

void LifecycleRuntime::resetStop() noexcept {
    stopping_.store(false);
}

bool LifecycleRuntime::isStopping() const noexcept {
    return stopping_.load();
}

}  // namespace ludork::global::scene_base_impl
