#pragma once

#include <GlobalRuntimeApi.hpp>
#include <Runtime/RuntimeObject.hpp>

#include <chrono>
#include <memory>
#include <string>

class Server;

/// Opaque account-scoped capability, valid for one write for at most 60
/// seconds.
BIND_CLASS()
class LUDORK_GLOBAL_API ServerWriteToken : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(ServerWriteToken, RuntimeObject)

    ServerWriteToken(const ServerWriteToken&) = delete;
    ServerWriteToken& operator=(const ServerWriteToken&) = delete;

private:
    friend class Server;
    ServerWriteToken(std::string account, std::string token,
                     std::chrono::steady_clock::time_point expires,
                     std::weak_ptr<void> session);

    std::string account_;
    std::string token_;
    std::chrono::steady_clock::time_point expires_;
    std::weak_ptr<void> session_;
    bool consumed_ = false;
};
