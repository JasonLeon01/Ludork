#pragma once

#include <atomic>
#include <functional>
#include <mutex>

namespace ludork::global::system_impl {

class LifecycleImpl {
public:
    bool isDebugMode() const;
    void setDebugMode(bool debugMode);
    void setStandardUpdate(std::function<void()> update);
    void updateRuntime();
    void initializeRuntimeSession() noexcept;
    bool isShuttingDown() const noexcept;
    void shutdownRuntime(const std::function<void()>& cleanup) noexcept;

private:
    std::function<void()> standardUpdate_;
    std::atomic_bool shuttingDown_ = false;
    std::mutex lifecycleMutex_;
    bool debugMode_ = false;
};

}  // namespace ludork::global::system_impl
