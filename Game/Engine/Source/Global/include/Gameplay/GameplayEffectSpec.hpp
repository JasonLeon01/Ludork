#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

class GameplayEventData;
class GameplayEffect;

BIND_CLASS()
class LUDORK_GLOBAL_API GameplayEffectSpec : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(GameplayEffectSpec, RuntimeObject)

    BIND_INIT(defaults = {nil, 1, nil},
              parameter_types = {GameplayEffect, GameplayEventData, int, any})
    GameplayEffectSpec(std::shared_ptr<GameplayEffect> gameplayEffect,
                       std::shared_ptr<GameplayEventData> gameplayEvent = {},
                       int stackCount = 1, RuntimeValue source = {});

    BIND_PROPERTY()
    std::shared_ptr<GameplayEffect> effect;

    BIND_PROPERTY()
    std::shared_ptr<GameplayEventData> eventData;

    BIND_PROPERTY()
    int stacks = 1;

    BIND_PROPERTY(type = any)
    RuntimeValue sourceKey;
};
