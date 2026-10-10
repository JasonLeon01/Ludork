#pragma once

#include <Server/ServerResult.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerAccountResult : public ServerResult {
public:
    LUDORK_CAST_DERIVED(ServerAccountResult, ServerResult)

    ServerAccountResult(ServerErrorCode code, std::string message,
                        bool exists = false);

    /// Whether the account data directory exists; meaningful only on success.
    BIND_METHOD(property = "exists")
    bool getExists() const;

private:
    bool exists_;
};
