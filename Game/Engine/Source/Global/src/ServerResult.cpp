#include <Server/ServerResult.hpp>

#include <utility>

ServerResult::ServerResult(ServerErrorCode code, std::string message)
    : code_(code), message_(std::move(message)) {}

bool ServerResult::getOk() const {
    return code_ == ServerErrorCode::None;
}
ServerErrorCode ServerResult::getCode() const {
    return code_;
}
std::string ServerResult::getMessage() const {
    return message_;
}
