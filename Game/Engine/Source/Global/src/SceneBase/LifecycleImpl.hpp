#pragma once

#include <atomic>

namespace ludork::global::scene_base_impl {

class LifecycleImpl {
public:
    [[nodiscard]] bool tryStartMain() noexcept;
    void finishMain() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;

    [[nodiscard]] bool isCreated() const noexcept;
    [[nodiscard]] bool isEntered() const noexcept;
    [[nodiscard]] bool isDestroyed() const noexcept;
    void markCreated() noexcept;
    void markEntered() noexcept;
    void markExited() noexcept;
    void markDestroyed() noexcept;
    void shutdown() noexcept;

    void requestStop() noexcept;
    void resetStop() noexcept;
    [[nodiscard]] bool isStopping() const noexcept;

    void setSuspended(bool suspended) noexcept;
    [[nodiscard]] bool isSuspended() const noexcept;
    [[nodiscard]] bool takeLogicTimeReset() noexcept;

private:
    bool created_ = false;
    bool entered_ = false;
    bool destroyed_ = false;
    std::atomic_bool stopping_{false};
    std::atomic_bool mainRunning_{false};
    std::atomic_bool suspended_{false};
    std::atomic_bool logicTimeResetPending_{false};
};

}  // namespace ludork::global::scene_base_impl
