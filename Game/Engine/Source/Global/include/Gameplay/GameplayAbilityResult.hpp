#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayAbilityResult : public RuntimeObject {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CODE_SUCCESS;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CODE_ABILITY_NOT_FOUND;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CODE_ABILITY_NOT_ACTIVATED;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CODE_MISSING_REQUIRED_TAG;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string CODE_BLOCKED_BY_TAG;

    LUDORK_CAST_DERIVED(GameplayAbilityResult, RuntimeObject)

    using Code = std::variant<std::string, std::int64_t>;

    BIND_INIT(defaults = {nil, nil})
    explicit GameplayAbilityResult(bool succeeded,
                                   std::optional<Code> resultCode = {},
                                   RuntimeIdentityPtr resultData = {});

    BIND_PROPERTY()
    bool ok = false;

    BIND_PROPERTY()
    Code code;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr data;

    BIND_METHOD(Pure = true, defaults = {nil, nil})
    static std::shared_ptr<GameplayAbilityResult> Success(
        std::optional<Code> code = {}, RuntimeIdentityPtr data = {});

    BIND_METHOD(Pure = true, defaults = {nil})
    static std::shared_ptr<GameplayAbilityResult> Failure(
        Code code, RuntimeIdentityPtr data = {});
};
