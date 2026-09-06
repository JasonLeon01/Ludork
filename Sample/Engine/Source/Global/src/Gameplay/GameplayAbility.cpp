#include <Gameplay/GameplayAbility.hpp>
#include <Gameplay/GameplayAbilityResult.hpp>
#include <Gameplay/GameplayEventData.hpp>
#include <Gameplay/AbilitySystemComponent.hpp>
#include "GameplayValueUtils.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

std::shared_ptr<GameplayAbilityResult> GameplayAbility::canActivate(
    const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
    const std::shared_ptr<GameplayEventData>&) {
    for (const std::string& tag : requiredTags) {
        if (!abilitySystem->hasMatchingGameplayTag(tag)) {
            return GameplayAbilityResult::Failure(
                RuntimeValue("MissingRequiredTag"),
                ludork::global::gameplay_detail::runtimeMap(
                    {{"tag", RuntimeValue(tag)}}));
        }
    }
    for (const std::string& tag : blockedTags) {
        if (abilitySystem->hasMatchingGameplayTag(tag)) {
            return GameplayAbilityResult::Failure(
                RuntimeValue("BlockedByTag"),
                ludork::global::gameplay_detail::runtimeMap(
                    {{"tag", RuntimeValue(tag)}}));
        }
    }
    return GameplayAbilityResult::Success();
}

std::shared_ptr<GameplayAbilityResult> GameplayAbility::calculate(
    const std::shared_ptr<AbilitySystemComponent>&,
    const std::shared_ptr<GameplayEventData>&) {
    return GameplayAbilityResult::Success();
}

std::shared_ptr<GameplayAbilityResult> GameplayAbility::activate(
    const std::shared_ptr<AbilitySystemComponent>& abilitySystem,
    const std::shared_ptr<GameplayEventData>& eventData) {
    return calculate(abilitySystem, eventData);
}
