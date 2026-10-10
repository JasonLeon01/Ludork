#pragma once

#include <Runtime/RuntimeData.hpp>
#include <Server/ServerResult.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerReadResult : public ServerResult {
public:
    LUDORK_CAST_DERIVED(ServerReadResult, ServerResult)

    ServerReadResult(ServerErrorCode code, std::string message,
                     bool found = false, RuntimeData value = {});

    BIND_METHOD(property = "found")
    bool getFound() const;
    BIND_METHOD(property = "value")
    RuntimeData getValue() const;

private:
    bool found_;
    RuntimeData value_;
};
