#pragma once
#include <Server/ServerResult.hpp>
#include <vector>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerAccountListResult : public ServerResult {
public:
    LUDORK_CAST_DERIVED(ServerAccountListResult, ServerResult)
    ServerAccountListResult(ServerErrorCode code, std::string message,
                            std::vector<std::string> accounts = {});
    BIND_METHOD(property = "accounts")
    std::vector<std::string> getAccounts() const;

private:
    std::vector<std::string> accounts_;
};
