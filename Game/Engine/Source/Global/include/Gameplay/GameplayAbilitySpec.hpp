#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

class GameplayAbility;

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayAbilitySpec : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(GameplayAbilitySpec, RuntimeObject)

    BIND_INIT(defaults = {nil, 0},
              parameter_types = {GameplayAbility, any, int})
    GameplayAbilitySpec(std::shared_ptr<GameplayAbility> gameplayAbility,
                        RuntimeValue source = {}, int order = 0);

    BIND_PROPERTY()
    std::shared_ptr<GameplayAbility> ability;

    BIND_PROPERTY(type = any)
    RuntimeValue sourceKey;

    BIND_PROPERTY()
    int grantOrder = 0;
};
