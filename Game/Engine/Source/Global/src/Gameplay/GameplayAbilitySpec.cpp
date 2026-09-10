#include <Gameplay/GameplayAbilitySpec.hpp>
#include <Gameplay/GameplayAbility.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

GameplayAbilitySpec::GameplayAbilitySpec(
    std::shared_ptr<GameplayAbility> gameplayAbility, RuntimeValue source,
    int order)
    : ability(std::move(gameplayAbility)),
      sourceKey(std::move(source)),
      grantOrder(order) {
    if (ability == nullptr) {
        throw std::invalid_argument("Gameplay Ability is required");
    }
}
