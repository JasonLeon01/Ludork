#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

class GameplayEffectSpec;
class GameplayAbilitySpec;

BIND_CLASS()
class LUDORK_GLOBAL_API ActiveGameplayEffect : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(ActiveGameplayEffect, RuntimeObject)

    BIND_INIT()
    ActiveGameplayEffect(int effectHandle,
                         std::shared_ptr<GameplayEffectSpec> effectSpec,
                         int order);

    BIND_PROPERTY()
    int handle = 0;

    BIND_PROPERTY()
    std::shared_ptr<GameplayEffectSpec> spec;

    BIND_PROPERTY()
    int stacks = 1;

    BIND_PROPERTY()
    int applicationOrder = 0;

    BIND_PROPERTY()
    std::vector<std::shared_ptr<GameplayAbilitySpec>> grantedAbilitySpecs;
};
