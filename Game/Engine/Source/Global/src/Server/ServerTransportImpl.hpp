#pragma once

#include "ServerRequestData.hpp"
#if defined(__EMSCRIPTEN__)
#include <Runtime/Async/AsyncOperation.hpp>
#endif

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <thread>
#include <vector>

namespace ludork::global::server_impl {

class ServerWebRequestImpl;

class ServerTransportImpl {
public:
    ServerTransportImpl();
    ~ServerTransportImpl();
    void enqueue(const std::shared_ptr<PendingRequest>& pending);

private:
    static constexpr int WorkerCount = 4;
    static constexpr std::size_t RequestQueueCapacity = 256;

#if defined(__EMSCRIPTEN__)
    void poll();
    std::shared_ptr<AsyncOperation> polling_;
    std::vector<std::unique_ptr<ServerWebRequestImpl>> requests_;
#else
    void run();
    std::condition_variable wake_;
    std::vector<std::thread> threads_;
#endif
    std::mutex mutex_;
    bool stopping_ = false;
    std::deque<std::shared_ptr<PendingRequest>> queue_;
    std::vector<std::shared_ptr<PendingRequest>> active_;
};

ServerTransportImpl& transport();
void initializeTransport();
void shutdownTransport() noexcept;

}  // namespace ludork::global::server_impl
