#pragma once

#include <GlobalRuntimeApi.hpp>
#include <Runtime/RuntimeObject.hpp>
#include <Server/ServerErrorCode.hpp>

#include <string>
#include <cstdint>
#include <optional>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerResult : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(ServerResult, RuntimeObject)

    ServerResult(ServerErrorCode code, std::string message,
                 std::optional<std::int64_t> retryAfterSeconds = {});

    BIND_METHOD(property = "ok")
    bool getOk() const;
    BIND_METHOD(property = "code")
    ServerErrorCode getCode() const;
    BIND_METHOD(property = "message")
    std::string getMessage() const;
    /// Present only when a write is rejected by the rate limit.
    BIND_METHOD(property = "retryAfterSeconds")
    std::optional<std::int64_t> getRetryAfterSeconds() const;

private:
    ServerErrorCode code_;
    std::string message_;
    std::optional<std::int64_t> retryAfterSeconds_;
};
