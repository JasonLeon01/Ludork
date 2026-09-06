#include <Gameplay/GameplayEffect.hpp>
#include "GameplayValueUtils.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

GameplayEffect::GameplayEffect()
    : data(ludork::global::gameplay_detail::runtimeMap()) {}
