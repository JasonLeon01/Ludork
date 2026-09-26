#include "LifecycleImpl.hpp"

namespace ludork::global::scene_base_impl {

bool LifecycleImpl::tryStartMain() noexcept {
    return !destroyed_ && !mainRunning_.exchange(true);
}

void LifecycleImpl::finishMain() noexcept {
    mainRunning_.store(false);
}

bool LifecycleImpl::isRunning() const noexcept {
    return mainRunning_.load();
}

bool LifecycleImpl::isCreated() const noexcept {
    return created_;
}

bool LifecycleImpl::isEntered() const noexcept {
    return entered_;
}

bool LifecycleImpl::isDestroyed() const noexcept {
    return destroyed_;
}

void LifecycleImpl::markCreated() noexcept {
    created_ = true;
}

void LifecycleImpl::markEntered() noexcept {
    entered_ = true;
    stopping_.store(false);
    suspended_.store(false);
    logicTimeResetPending_.store(false);
}

void LifecycleImpl::markExited() noexcept {
    entered_ = false;
}

void LifecycleImpl::markDestroyed() noexcept {
    destroyed_ = true;
}

void LifecycleImpl::shutdown() noexcept {
    entered_ = false;
    destroyed_ = true;
}

void LifecycleImpl::requestStop() noexcept {
    stopping_.store(true);
}

void LifecycleImpl::resetStop() noexcept {
    stopping_.store(false);
}

bool LifecycleImpl::isStopping() const noexcept {
    return stopping_.load();
}

void LifecycleImpl::setSuspended(bool suspended) noexcept {
    if (suspended) {
        logicTimeResetPending_.store(true);
    }
    suspended_.store(suspended);
}

bool LifecycleImpl::isSuspended() const noexcept {
    return suspended_.load();
}

bool LifecycleImpl::takeLogicTimeReset() noexcept {
    return logicTimeResetPending_.exchange(false);
}

}  // namespace ludork::global::scene_base_impl
