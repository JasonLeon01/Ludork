#pragma once

#include <atomic>

namespace ludork::global::scene_base_impl {

class LifecycleRuntime {
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

private:
    bool created_ = false;
    bool entered_ = false;
    bool destroyed_ = false;
    std::atomic_bool stopping_{false};
    std::atomic_bool mainRunning_{false};
};

}  // namespace ludork::global::scene_base_impl
