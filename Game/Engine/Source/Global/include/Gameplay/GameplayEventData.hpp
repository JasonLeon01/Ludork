#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayEventData : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(GameplayEventData, RuntimeObject)

    BIND_INIT(defaults = {nil, nil, "", nil},
              parameter_types = {any, any, string, any})
    explicit GameplayEventData(RuntimeValue eventInstigator = {},
                               RuntimeValue eventTarget = {},
                               std::string tag = "",
                               RuntimeIdentityPtr eventPayload = {});

    BIND_PROPERTY(type = any)
    RuntimeValue instigator;

    BIND_PROPERTY(type = any)
    RuntimeValue target;

    BIND_PROPERTY()
    std::string eventTag;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr payload;
};
