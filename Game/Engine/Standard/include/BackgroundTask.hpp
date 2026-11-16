#pragma once

#include <StandardApi.hpp>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace ludork::standard {

class LUDORK_STANDARD_API BackgroundTask {
public:
    BackgroundTask() noexcept;
    explicit BackgroundTask(std::function<void()> work);
    ~BackgroundTask();
    BackgroundTask(BackgroundTask&&) noexcept;
    BackgroundTask& operator=(BackgroundTask&&) noexcept;
    BackgroundTask(const BackgroundTask&) = delete;
    BackgroundTask& operator=(const BackgroundTask&) = delete;

    bool joinable() const noexcept;
    void join();
    static void poll(double milliseconds = 2.0);
    static void yield();

    template <class Predicate>
    static void wait(std::condition_variable& condition,
                     std::unique_lock<std::mutex>& lock, Predicate ready) {
#ifdef __EMSCRIPTEN__
        static_cast<void>(condition);
        while (!ready()) {
            lock.unlock();
            poll();
            yield();
            lock.lock();
        }
#else
        condition.wait(lock, std::move(ready));
#endif
    }

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace ludork::standard
