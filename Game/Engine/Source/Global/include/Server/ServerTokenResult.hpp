#pragma once

#include <Server/ServerResult.hpp>
#include <Server/ServerWriteToken.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerTokenResult : public ServerResult {
public:
    LUDORK_CAST_DERIVED(ServerTokenResult, ServerResult)

    ServerTokenResult(ServerErrorCode code, std::string message,
                      std::shared_ptr<ServerWriteToken> token = {});

    BIND_METHOD(property = "token")
    std::shared_ptr<ServerWriteToken> getToken() const;

private:
    std::shared_ptr<ServerWriteToken> token_;
};
