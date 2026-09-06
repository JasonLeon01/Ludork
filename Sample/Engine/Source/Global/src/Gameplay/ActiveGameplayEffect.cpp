#include <Gameplay/ActiveGameplayEffect.hpp>
#include <Gameplay/GameplayEffectSpec.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

ActiveGameplayEffect::ActiveGameplayEffect(
    int effectHandle, std::shared_ptr<GameplayEffectSpec> effectSpec, int order)
    : handle(effectHandle),
      spec(std::move(effectSpec)),
      stacks(spec == nullptr ? 1 : spec->stacks),
      applicationOrder(order) {
    if (spec == nullptr) {
        throw std::invalid_argument("Gameplay Effect Spec is required");
    }
}
