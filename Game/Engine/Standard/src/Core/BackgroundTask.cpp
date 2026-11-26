#include <BackgroundTask.hpp>
#include "BackgroundTaskImpl.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include "BatchWorkerScope.hpp"
#endif

namespace ludork::standard {

#ifdef __EMSCRIPTEN__
BackgroundTask::Impl* BackgroundTask::Impl::active = nullptr;
emscripten_fiber_t BackgroundTask::Impl::mainFiber{};
alignas(16) std::array<std::byte, 65536> BackgroundTask::Impl::mainStack{};

std::vector<std::weak_ptr<BackgroundTask::Impl>>&
BackgroundTask::Impl::tasks() {
    static std::vector<std::weak_ptr<Impl>> value;
    return value;
}

BackgroundTask::Impl::Impl(std::function<void()> function)
    : work(std::move(function)), stack(CStackSize + 15), asyncifyStack(65536) {
    void* cStack = stack.data();
    std::size_t available = stack.size();
    if (std::align(16, CStackSize, cStack, available) == nullptr) {
        throw std::bad_alloc();
    }
    emscripten_fiber_init(&fiber, enter, this, cStack, CStackSize,
                          asyncifyStack.data(), asyncifyStack.size());
}

void BackgroundTask::Impl::enter(void* value) {
    auto& task = *static_cast<Impl*>(value);
    task.work();
    task.work = {};
    task.finished = true;
    emscripten_fiber_swap(&task.fiber, task.caller);
    std::terminate();
}

void BackgroundTask::Impl::resume() {
    if (finished) {
        return;
    }
    Impl* previous = active;
    if (previous == this) {
        throw std::logic_error("A background task cannot wait for itself");
    }
    if (previous == nullptr) {
        emscripten_fiber_init_from_current_context(&mainFiber, mainStack.data(),
                                                   mainStack.size());
    }
    caller = previous == nullptr ? &mainFiber : &previous->fiber;
    active = this;
    emscripten_fiber_swap(caller, &fiber);
    active = previous;
}
#else
BackgroundTask::Impl::Impl(std::function<void()> work)
    : worker(runWithBatchWorkerScope, std::move(work)) {}
#endif

BackgroundTask::BackgroundTask() noexcept = default;
BackgroundTask::BackgroundTask(std::function<void()> work)
    : impl_(std::make_shared<Impl>(std::move(work))) {
#ifdef __EMSCRIPTEN__
    Impl::tasks().push_back(impl_);
#endif
}
BackgroundTask::~BackgroundTask() {
    if (joinable()) {
        join();
    }
}
BackgroundTask::BackgroundTask(BackgroundTask&&) noexcept = default;
BackgroundTask& BackgroundTask::operator=(BackgroundTask&& other) noexcept {
    if (this != &other) {
        if (joinable()) {
            join();
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool BackgroundTask::joinable() const noexcept {
#ifdef __EMSCRIPTEN__
    return impl_ && !impl_->joined;
#else
    return impl_ && impl_->worker.joinable();
#endif
}

void BackgroundTask::join() {
    if (!joinable()) {
        return;
    }
#ifdef __EMSCRIPTEN__
    while (!impl_->finished) {
        impl_->resume();
        if (!impl_->finished) {
            poll();
            yield();
        }
    }
    impl_->joined = true;
#else
    impl_->worker.join();
#endif
}

void BackgroundTask::poll(double milliseconds) {
#ifdef __EMSCRIPTEN__
    if (Impl::active != nullptr) {
        return;
    }
    auto& tasks = Impl::tasks();
    std::erase_if(tasks, [](const auto& task) {
        return task.expired();
    });
    const auto started = std::chrono::steady_clock::now();
    const auto snapshot = tasks;
    for (const auto& weak : snapshot) {
        if (const auto task = weak.lock(); task && !task->finished) {
            task->resume();
        }
        if (std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started)
                .count() >= milliseconds) {
            const auto position =
                std::find_if(tasks.begin(), tasks.end(), [&](const auto& task) {
                    return !task.owner_before(weak) && !weak.owner_before(task);
                });
            if (position != tasks.end()) {
                std::rotate(tasks.begin(), position + 1, tasks.end());
            }
            break;
        }
    }
#else
    static_cast<void>(milliseconds);
#endif
}

void BackgroundTask::yield() {
#ifdef __EMSCRIPTEN__
    if (Impl::active != nullptr) {
        emscripten_fiber_swap(&Impl::active->fiber, Impl::active->caller);
    } else {
        emscripten_sleep(0);
    }
#endif
}

}  // namespace ludork::standard
