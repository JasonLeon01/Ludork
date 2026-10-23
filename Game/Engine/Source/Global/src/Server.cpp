#include <LudorkGenerated/ServerConfig.hpp>
#include <NetTransport.hpp>
#include <Server.hpp>

#include "Server/ServerRequestData.hpp"
#if defined(LUDORK_SERVER_AVAILABLE)
#include "Server/ServerTransportImpl.hpp"
#endif

#include <stdexcept>
#include <utility>

std::shared_ptr<AsyncOperation> Server::CheckConnectionAsync(
    ConnectionCallback onCompleted) {
    if (!onCompleted) {
        throw std::invalid_argument(
            "Server operation requires a completion callback.");
    }
    const auto pending =
        std::make_shared<ludork::global::server_impl::PendingRequest>();
    pending->request = {"GET", "/health"};
    pending->request.health = true;
    pending->deadline = pending->started + std::chrono::seconds(3);
#if !defined(LUDORK_SERVER_AVAILABLE)
    pending->response = ludork::global::server_impl::Response{
        ServerErrorCode::Disabled,
        "Ludork Server is disabled for this project."};
#endif
    const auto operation = AsyncOperation::create(
        [pending, callback = std::move(onCompleted)](AsyncOperation& current) {
            std::optional<ludork::global::server_impl::Response> response;
            {
                std::lock_guard lock(pending->mutex);
                if (pending->response) {
                    response = std::move(pending->response);
                    pending->response.reset();
                }
            }
            if (!response &&
                ludork::global::server_impl::PendingRequest::Clock::now() >=
                    pending->deadline) {
                pending->cancelled.store(true);
                response = ludork::global::server_impl::Response{
                    ServerErrorCode::Timeout, "The server request timed out."};
            }
            if (!response) {
                return;
            }
            const std::shared_ptr<ServerConnectionResult> result =
                std::make_shared<ServerConnectionResult>(
                    response->code, std::move(response->message),
                    response->code == ServerErrorCode::None
                        ? std::optional(response->roundTripMs)
                        : std::nullopt);
            current.complete(
                RuntimeValue(std::static_pointer_cast<RuntimeObject>(result)));
            callback(result);
        });
    operation->onCancelled([pending] {
        pending->cancelled.store(true);
    });
#if defined(LUDORK_SERVER_AVAILABLE)
    ludork::global::server_impl::transport().enqueue(pending);
#endif
    return operation;
}

namespace ludork::global {

void initializeServer() {
    initializeNetTransport();
#if defined(LUDORK_SERVER_AVAILABLE)
    server_impl::initializeTransport();
#endif
}

void shutdownServer() noexcept {
    shutdownNetTransport();
#if defined(LUDORK_SERVER_AVAILABLE)
    server_impl::shutdownTransport();
#endif
}

}  // namespace ludork::global
