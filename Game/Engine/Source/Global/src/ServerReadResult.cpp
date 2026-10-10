#include <Server/ServerReadResult.hpp>

#include <utility>

ServerReadResult::ServerReadResult(ServerErrorCode code, std::string message,
                                   bool found, RuntimeData value)
    : ServerResult(code, std::move(message)),
      found_(found),
      value_(std::move(value)) {}

bool ServerReadResult::getFound() const {
    return found_;
}
RuntimeData ServerReadResult::getValue() const {
    return value_;
}
