#include <Server/ServerAccountResult.hpp>

#include <utility>

ServerAccountResult::ServerAccountResult(ServerErrorCode code,
                                         std::string message, bool exists)
    : ServerResult(code, std::move(message)), exists_(exists) {}

bool ServerAccountResult::getExists() const {
    return exists_;
}
