#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayAbilityResult : public RuntimeObject {
public:
    BIND_INIT(defaults = {nil, nil}, parameter_types = {bool, any, any})
    explicit GameplayAbilityResult(bool succeeded, RuntimeValue resultCode = {},
                                   RuntimeIdentityPtr resultData = {});

    BIND_PROPERTY()
    bool ok = false;

    BIND_PROPERTY(type = any)
    RuntimeValue code;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr data;

    BIND_METHOD(Pure = true, defaults = {nil, nil},
                parameter_types = {any, any})
    static std::shared_ptr<GameplayAbilityResult> Success(
        RuntimeValue code = {}, RuntimeIdentityPtr data = {});

    BIND_METHOD(Pure = true, defaults = {nil}, parameter_types = {any, any})
    static std::shared_ptr<GameplayAbilityResult> Failure(
        RuntimeValue code, RuntimeIdentityPtr data = {});
};
