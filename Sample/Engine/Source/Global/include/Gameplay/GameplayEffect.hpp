#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Gameplay/GameplayModifier.hpp>

class GameplayAbility;

BIND_CLASS(table_init = true)
class LUDORK_GLOBAL_API GameplayEffect : public RuntimeObject {
public:
    GameplayEffect();

    BIND_PROPERTY()
    std::string id;

    BIND_PROPERTY()
    std::string durationPolicy = "Instant";

    BIND_PROPERTY()
    std::string stackingPolicy = "None";

    BIND_PROPERTY()
    std::vector<GameplayModifier> modifiers;

    BIND_PROPERTY()
    std::vector<std::string> grantedTags;

    BIND_PROPERTY()
    std::vector<std::shared_ptr<GameplayAbility>> grantedAbilities;

    BIND_PROPERTY(type = any)
    RuntimeIdentityPtr data;
};
