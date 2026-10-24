#pragma once

#include <Runtime/Async/AsyncOperation.hpp>
#include <Runtime/StrictFunction.hpp>
#include <Server/ServerConnectionResult.hpp>

/// Connection diagnostics for the server configured when packaging the project.
BIND_CLASS()
class LUDORK_GLOBAL_API Server {
public:
    using ConnectionCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerConnectionResult>)>;

    /// Probe the configured service. Normal is at most 300 ms, Weak at most
    /// 3000 ms, and Disconnected means no valid health response in that time.
    /// When disabled, log a diagnostic and return null without invoking the
    /// callback.
    BIND_METHOD()
    static std::shared_ptr<AsyncOperation> CheckConnectionAsync(
        ConnectionCallback onCompleted);
};

namespace ludork::global {
void initializeServer();
void shutdownServer() noexcept;
}  // namespace ludork::global
