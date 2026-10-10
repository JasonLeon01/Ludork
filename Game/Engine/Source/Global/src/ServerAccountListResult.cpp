#include <Server/ServerAccountListResult.hpp>
#include <utility>
ServerAccountListResult::ServerAccountListResult(
    ServerErrorCode code, std::string message,
    std::vector<std::string> accounts)
    : ServerResult(code, std::move(message)), accounts_(std::move(accounts)) {}
std::vector<std::string> ServerAccountListResult::getAccounts() const {
    return accounts_;
}
