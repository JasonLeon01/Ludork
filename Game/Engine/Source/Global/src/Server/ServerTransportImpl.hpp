#pragma once

#include "ServerRequestData.hpp"

#include <condition_variable>
#include <deque>
#include <thread>
#include <vector>

namespace ludork::global::server_impl {

class ServerTransportImpl {
public:
    ServerTransportImpl();
    ~ServerTransportImpl();
    void enqueue(const std::shared_ptr<PendingRequest>& pending);

private:
    void run();
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    std::deque<std::shared_ptr<PendingRequest>> queue_;
    std::vector<std::shared_ptr<PendingRequest>> active_;
    std::vector<std::thread> threads_;
};

ServerTransportImpl& transport();
void initializeTransport();
void shutdownTransport() noexcept;

}  // namespace ludork::global::server_impl
