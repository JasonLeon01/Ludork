#pragma once

#include <BatchWorkerPool.hpp>

#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace ludork::standard {

struct BatchWorkerPool::Impl {
    ~Impl();

    static std::shared_ptr<Impl> acquire();
    void run(std::size_t count, const std::function<void(std::size_t)>& work);

private:
    struct Batch {
        std::size_t count;
        const std::function<void(std::size_t)>& work;
        std::size_t next{};
        std::size_t active{};
        std::exception_ptr failure;
        std::condition_variable finished;
    };

    void ensureWorkers(std::size_t count);
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Batch*> queued_;
    std::vector<std::thread> workers_;
    std::size_t busy_{};
    bool stopping_{};
};

}  // namespace ludork::standard
