#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Gameplay/GameplayModifier.hpp>

class GameplayAbility;

BIND_CLASS(table_init = true)
class LUDORK_GLOBAL_API GameplayEffect : public RuntimeObject {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string DURATION_INSTANT;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string DURATION_INFINITE;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string STACKING_NONE;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string STACKING_AGGREGATE;

    LUDORK_CAST_DERIVED(GameplayEffect, RuntimeObject)

    GameplayEffect();

    BIND_PROPERTY()
    std::string id;

    BIND_PROPERTY(default = "Instant")
    std::string durationPolicy = DURATION_INSTANT;

    BIND_PROPERTY(default = "None")
    std::string stackingPolicy = STACKING_NONE;

    BIND_PROPERTY()
    std::vector<GameplayModifier> modifiers;

    BIND_PROPERTY()
    std::vector<std::string> grantedTags;

    BIND_PROPERTY()
    std::vector<std::shared_ptr<GameplayAbility>> grantedAbilities;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr data;
};
