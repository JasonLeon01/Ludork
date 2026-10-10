#include <LudorkGenerated/ServerConfig.hpp>

#if defined(LUDORK_SERVER_AVAILABLE)
#include "ServerTransportImpl.hpp"
#include "ServerHttpTransportImpl.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>

namespace ludork::global::server_impl {
namespace {
std::unique_ptr<ServerTransportImpl> instance;
}

ServerTransportImpl::ServerTransportImpl() {
    try {
        for (int index = 0; index < 4; ++index) {
            threads_.emplace_back([this] {
                run();
            });
        }
    } catch (...) {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        for (std::thread& thread : threads_) {
            thread.join();
        }
        throw;
    }
}

ServerTransportImpl::~ServerTransportImpl() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        for (const auto& request : queue_) {
            request->cancelled.store(true);
        }
        for (const auto& request : active_) {
            request->cancelled.store(true);
        }
        queue_.clear();
    }
    wake_.notify_all();
    for (std::thread& thread : threads_) {
        thread.join();
    }
}

void ServerTransportImpl::enqueue(
    const std::shared_ptr<PendingRequest>& pending) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || queue_.size() >= 256) {
            std::lock_guard resultLock(pending->mutex);
            pending->response =
                Response{ServerErrorCode::ConnectionFailed,
                         stopping_ ? "Server transport is stopping."
                                   : "Server request queue is full."};
            return;
        }
        queue_.push_back(pending);
    }
    wake_.notify_one();
}

std::shared_ptr<void> ServerTransportImpl::session() const {
    return session_;
}

void ServerTransportImpl::run() {
    for (;;) {
        std::shared_ptr<PendingRequest> pending;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] {
                return stopping_ || !queue_.empty();
            });
            if (stopping_) {
                return;
            }
            pending = queue_.front();
            queue_.pop_front();
            active_.push_back(pending);
        }
        Response response;
        if (!pending->cancelled.load()) {
            try {
                response = ServerHttpTransportImpl(*pending).perform();
            } catch (const std::invalid_argument&) {
                response.code = ServerErrorCode::InvalidArgument;
                response.message = "The compiled server URL is invalid.";
            } catch (const std::exception&) {
                response.code = ServerErrorCode::ConnectionFailed;
                response.message = "The server request could not be completed.";
            }
            std::lock_guard resultLock(pending->mutex);
            if (!pending->cancelled.load()) {
                pending->response = std::move(response);
            }
        }
        {
            std::lock_guard lock(mutex_);
            std::erase(active_, pending);
        }
    }
}

ServerTransportImpl& transport() {
    if (!instance) {
        throw std::logic_error("Server transport requires a running session.");
    }
    return *instance;
}
void initializeTransport() {
    if (!instance) {
        instance = std::make_unique<ServerTransportImpl>();
    }
}
void shutdownTransport() noexcept {
    instance.reset();
}

}  // namespace ludork::global::server_impl
#endif
