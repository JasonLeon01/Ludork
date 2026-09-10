#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

class GameplayAbilityResult;
class GameplayEventData;
class AbilitySystemComponent;

BIND_CLASS(callbacks = true, table_init = true)
class LUDORK_GLOBAL_API GameplayAbility : public RuntimeObject {
public:
    virtual ~GameplayAbility() = default;

    BIND_PROPERTY()
    std::string id;

    BIND_PROPERTY()
    double priority = 0.0;

    BIND_PROPERTY()
    std::vector<std::string> abilityTags;

    BIND_PROPERTY()
    std::vector<std::string> requiredTags;

    BIND_PROPERTY()
    std::vector<std::string> blockedTags;

    BIND_PROPERTY()
    std::vector<std::string> triggerTags;

    BIND_METHOD(Pure = true,
                parameter_types = {AbilitySystemComponent, GameplayEventData})
    virtual std::shared_ptr<GameplayAbilityResult> canActivate(
        const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
        const std::shared_ptr<GameplayEventData>& eventData);

    BIND_METHOD(Pure = true,
                parameter_types = {AbilitySystemComponent, GameplayEventData})
    virtual std::shared_ptr<GameplayAbilityResult> calculate(
        const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
        const std::shared_ptr<GameplayEventData>& eventData);

    BIND_METHOD(parameter_types = {AbilitySystemComponent, GameplayEventData})
    virtual std::shared_ptr<GameplayAbilityResult> activate(
        const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
        const std::shared_ptr<GameplayEventData>& eventData);
};
