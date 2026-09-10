#include <Gameplay/GameplayEffectSpec.hpp>
#include <Gameplay/GameplayEventData.hpp>
#include <Gameplay/GameplayEffect.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

GameplayEffectSpec::GameplayEffectSpec(
    std::shared_ptr<GameplayEffect> gameplayEffect,
    std::shared_ptr<GameplayEventData> gameplayEvent, int stackCount,
    RuntimeValue source)
    : effect(std::move(gameplayEffect)),
      eventData(std::move(gameplayEvent)),
      stacks(stackCount),
      sourceKey(std::move(source)) {
    if (effect == nullptr) {
        throw std::invalid_argument("Gameplay Effect is required");
    }
    if (stacks <= 0) {
        throw std::invalid_argument("Gameplay Effect stacks must be positive");
    }
}
