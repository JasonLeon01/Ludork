#pragma once
#include "AbilitySystemImpl.hpp"
#include <Gameplay/GameplayEventData.hpp>

namespace ludork::global::ability_system_impl {

void validateAbility(const AbilitySystemImpl& state,
                     const std::shared_ptr<GameplayAbility>& ability);

std::shared_ptr<GameplayAbilitySpec> giveAbility(
    AbilitySystemImpl& state, std::shared_ptr<GameplayAbility> ability,
    RuntimeValue sourceKey = {});

void removeAbilitiesBySource(AbilitySystemImpl& state,
                             const RuntimeValue& sourceKey);

bool hasMatchingGameplayTag(const AbilitySystemImpl& state,
                            const std::string& tag);

bool tagMatches(const std::string& tag, const std::string& query);

std::vector<std::shared_ptr<GameplayAbilitySpec>> abilityCandidates(
    const AbilitySystemImpl& state, const std::string& abilityID);

std::vector<std::shared_ptr<GameplayAbilitySpec>> eventCandidates(
    const AbilitySystemImpl& state, const GameplayEventData& eventData);
}  // namespace ludork::global::ability_system_impl
