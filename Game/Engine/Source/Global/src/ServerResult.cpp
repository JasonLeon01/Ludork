#include <Server/ServerResult.hpp>

#include <utility>

ServerResult::ServerResult(ServerErrorCode code, std::string message,
                           std::optional<std::int64_t> retryAfterSeconds)
    : code_(code),
      message_(std::move(message)),
      retryAfterSeconds_(code == ServerErrorCode::RateLimited
                             ? retryAfterSeconds
                             : std::nullopt) {}

bool ServerResult::getOk() const {
    return code_ == ServerErrorCode::None;
}
ServerErrorCode ServerResult::getCode() const {
    return code_;
}
std::string ServerResult::getMessage() const {
    return message_;
}

std::optional<std::int64_t> ServerResult::getRetryAfterSeconds() const {
    return retryAfterSeconds_;
}
