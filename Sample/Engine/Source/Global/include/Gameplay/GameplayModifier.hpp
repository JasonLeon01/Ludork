#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Gameplay/GameplayNumber.hpp>
#include <Runtime/StrictFunction.hpp>

class GameplayEffectSpec;

BIND_CLASS(copyable = true, table_init = true)
struct GameplayModifier {
    using MagnitudeFunction = ludork::runtime::StrictFunction<GameplayNumber(
        const std::shared_ptr<GameplayEffectSpec>&, int)>;
    using Magnitude = std::variant<std::int64_t, double, MagnitudeFunction>;

    BIND_PROPERTY()
    std::string attribute;

    BIND_PROPERTY()
    std::string operation;

    BIND_PROPERTY()
    std::optional<Magnitude> magnitude;

    BIND_PROPERTY()
    std::optional<GameplayNumber> minimum;
};
