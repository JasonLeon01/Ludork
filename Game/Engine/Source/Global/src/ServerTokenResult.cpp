#include <Server/ServerTokenResult.hpp>

#include <utility>

ServerTokenResult::ServerTokenResult(ServerErrorCode code, std::string message,
                                     std::shared_ptr<ServerWriteToken> token)
    : ServerResult(code, std::move(message)), token_(std::move(token)) {}

std::shared_ptr<ServerWriteToken> ServerTokenResult::getToken() const {
    return token_;
}
