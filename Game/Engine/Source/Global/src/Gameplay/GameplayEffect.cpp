#include <Gameplay/GameplayEffect.hpp>
#include "GameplayValueUtils.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

const std::string GameplayEffect::DURATION_INSTANT = "Instant";
const std::string GameplayEffect::DURATION_INFINITE = "Infinite";
const std::string GameplayEffect::STACKING_NONE = "None";
const std::string GameplayEffect::STACKING_AGGREGATE = "Aggregate";

GameplayEffect::GameplayEffect()
    : data(ludork::global::gameplay_detail::runtimeMap()) {}
