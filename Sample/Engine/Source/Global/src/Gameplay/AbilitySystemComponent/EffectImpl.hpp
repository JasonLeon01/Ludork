#pragma once
#include "AbilitySystemImpl.hpp"
#include <Gameplay/GameplayEventData.hpp>

namespace ludork::global::ability_system_impl {

std::shared_ptr<ActiveGameplayEffect> activeEffect(
    const AbilitySystemImpl& state, int handle);

void validateEffectSpec(const AbilitySystemImpl& state,
                        const std::shared_ptr<GameplayEffectSpec>& spec);

std::shared_ptr<ActiveGameplayEffect> findStackableEffect(
    const AbilitySystemImpl& state,
    const std::shared_ptr<GameplayEffectSpec>& spec);

std::pair<AttributeNumbers, bool> instantBases(
    const AbilitySystemImpl& state,
    const std::shared_ptr<GameplayEffectSpec>& spec);

void changeTagCount(AbilitySystemImpl& state, const std::string& tag,
                    int delta);

bool validateGameplayEffectSpec(
    const AbilitySystemImpl& state,
    const std::shared_ptr<GameplayEffectSpec>& spec);

std::optional<int> applyGameplayEffectSpec(
    AbilitySystemImpl& state, const std::shared_ptr<GameplayEffectSpec>& spec);

bool removeActiveGameplayEffect(AbilitySystemImpl& state, int handle,
                                std::optional<int> stacks);

int getActiveEffectStacks(const AbilitySystemImpl& state,
                          const std::string& effectID);

std::vector<std::shared_ptr<ActiveGameplayEffect>> getActiveGameplayEffects(
    const AbilitySystemImpl& state);
}  // namespace ludork::global::ability_system_impl
