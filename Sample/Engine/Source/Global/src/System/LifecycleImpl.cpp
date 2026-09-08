#include "LifecycleImpl.hpp"
#include <EngineState.hpp>
#include <Manager/AudioManager.hpp>
#include <utility>

namespace ludork::global::system_impl {

bool LifecycleImpl::isDebugMode() const {
    return debugMode_;
}

void LifecycleImpl::setDebugMode(bool debugMode) {
    debugMode_ = debugMode;
}

void LifecycleImpl::setStandardUpdate(std::function<void()> update) {
    standardUpdate_ = std::move(update);
}

void LifecycleImpl::updateRuntime() {
    if (!shuttingDown_.load() && standardUpdate_) {
        try {
            standardUpdate_();
        } catch (...) {
            AudioManager::stopAll();
            throw;
        }
    }
}

void LifecycleImpl::initializeRuntimeSession() noexcept {
    const std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    shuttingDown_.store(false);
}

bool LifecycleImpl::isShuttingDown() const noexcept {
    return shuttingDown_.load();
}

void LifecycleImpl::shutdownRuntime(
    const std::function<void()>& cleanup) noexcept {
    const std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    shuttingDown_.store(true);
    cleanup();
    debugMode_ = false;
    engineState().setGameRunning(false);
}

}  // namespace ludork::global::system_impl
