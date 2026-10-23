#pragma once

#include <GlobalRuntimeApi.hpp>
#include <Runtime/RuntimeObject.hpp>
#include <Server/ServerErrorCode.hpp>

#include <string>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerResult : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(ServerResult, RuntimeObject)

    ServerResult(ServerErrorCode code, std::string message);

    BIND_METHOD(property = "ok")
    bool getOk() const;
    BIND_METHOD(property = "code")
    ServerErrorCode getCode() const;
    BIND_METHOD(property = "message")
    std::string getMessage() const;

private:
    ServerErrorCode code_;
    std::string message_;
};
