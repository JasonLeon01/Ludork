#include <BatchWorkerPool.hpp>

#ifdef __EMSCRIPTEN__
#include <BackgroundTask.hpp>
#include <chrono>
#else
#include "BatchWorkerPoolImpl.hpp"
#include <FileWorkers.hpp>
#include <algorithm>
#include <system_error>
#endif

namespace ludork::standard {

#ifdef __EMSCRIPTEN__
BatchWorkerPool::BatchWorkerPool() = default;

void BatchWorkerPool::run(std::size_t count,
                          const std::function<void(std::size_t)>& work) const {
    auto slice = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < count; ++index) {
        work(index);
        if (std::chrono::steady_clock::now() - slice >=
            std::chrono::milliseconds(2)) {
            BackgroundTask::yield();
            slice = std::chrono::steady_clock::now();
        }
    }
}
#else
namespace {
thread_local bool isBatchWorker = false;
}

BatchWorkerPool::BatchWorkerPool() : impl_(Impl::acquire()) {}

void BatchWorkerPool::run(std::size_t count,
                          const std::function<void(std::size_t)>& work) const {
    if (isBatchWorker) {
        for (std::size_t index = 0; index < count; ++index) {
            work(index);
        }
    } else if (count != 0) {
        impl_->run(count, work);
    }
}

std::shared_ptr<BatchWorkerPool::Impl> BatchWorkerPool::Impl::acquire() {
    static std::mutex mutex;
    static std::weak_ptr<Impl> current;
    std::lock_guard lock(mutex);
    auto pool = current.lock();
    if (!pool) {
        pool = std::make_shared<Impl>();
        current = pool;
    }
    return pool;
}

BatchWorkerPool::Impl::~Impl() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    ready_.notify_all();
    for (auto& worker : workers_) {
        worker.join();
    }
}

void BatchWorkerPool::Impl::ensureWorkers(std::size_t count) {
    const auto limit = fileWorkerCount();
    auto desired = busy_ + std::min(count, limit - busy_);
    for (const auto* batch : queued_) {
        desired += std::min(batch->count - batch->next, limit - desired);
    }
    while (workers_.size() < desired) {
        try {
            workers_.emplace_back([this] {
                workerLoop();
            });
        } catch (const std::system_error&) {
            if (workers_.empty()) {
                throw;
            }
            break;
        }
    }
}

void BatchWorkerPool::Impl::run(std::size_t count,
                                const std::function<void(std::size_t)>& work) {
    Batch batch{count, work};
    std::unique_lock lock(mutex_);
    ensureWorkers(count);
    queued_.push_back(&batch);
    ready_.notify_all();
    batch.finished.wait(lock, [&] {
        return batch.next == count && batch.active == 0;
    });
    if (batch.failure) {
        std::rethrow_exception(batch.failure);
    }
}

void BatchWorkerPool::Impl::workerLoop() {
    isBatchWorker = true;
    std::unique_lock lock(mutex_);
    for (;;) {
        ready_.wait(lock, [&] {
            return stopping_ || !queued_.empty();
        });
        if (stopping_) {
            return;
        }
        auto& batch = *queued_.front();
        const auto index = batch.next++;
        if (batch.next == batch.count) {
            queued_.pop_front();
        }
        ++batch.active;
        ++busy_;
        lock.unlock();

        std::exception_ptr failure;
        try {
            batch.work(index);
        } catch (...) {
            failure = std::current_exception();
        }

        lock.lock();
        --busy_;
        --batch.active;
        if (failure && !batch.failure) {
            batch.failure = failure;
            if (batch.next < batch.count) {
                std::erase(queued_, &batch);
                batch.next = batch.count;
            }
        }
        if (batch.next == batch.count && batch.active == 0) {
            batch.finished.notify_one();
        }
    }
}
#endif

BatchWorkerPool::~BatchWorkerPool() = default;

}  // namespace ludork::standard
