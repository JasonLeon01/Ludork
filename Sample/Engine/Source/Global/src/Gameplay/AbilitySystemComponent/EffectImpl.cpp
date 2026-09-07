#include "EffectImpl.hpp"
#include "AttributeImpl.hpp"
#include "AbilityImpl.hpp"
#include <Gameplay/GameplayEffect.hpp>
#include <algorithm>
#include <stdexcept>

namespace ludork::global::ability_system_impl {

std::shared_ptr<ActiveGameplayEffect> activeEffect(
    const AbilitySystemImpl& state, int handle) {
    const auto iterator = state.activeEffects.find(handle);
    return iterator == state.activeEffects.end() ? nullptr : iterator->second;
}

void validateEffectSpec(const AbilitySystemImpl& state,
                        const std::shared_ptr<GameplayEffectSpec>& spec) {
    if (spec == nullptr || spec->effect == nullptr) {
        throw std::invalid_argument(
            "Ability System requires GameplayEffectSpec");
    }
    if (spec->stacks <= 0) {
        throw std::invalid_argument("Gameplay Effect stacks must be positive");
    }
    const GameplayEffect& effect = *spec->effect;
    if (effect.durationPolicy != "Instant" &&
        effect.durationPolicy != "Infinite") {
        throw std::invalid_argument(
            "Unsupported Gameplay Effect duration policy: " +
            effect.durationPolicy);
    }
    if (effect.stackingPolicy != "None" &&
        effect.stackingPolicy != "Aggregate") {
        throw std::invalid_argument(
            "Unsupported Gameplay Effect stacking policy: " +
            effect.stackingPolicy);
    }
    if (effect.stackingPolicy == "None" && spec->stacks != 1) {
        throw std::invalid_argument(
            "Non-stacking Gameplay Effects require exactly one stack");
    }
    if (effect.stackingPolicy == "Aggregate" &&
        effect.durationPolicy == "Infinite" && effect.id.empty()) {
        throw std::invalid_argument(
            "Aggregate Infinite Gameplay Effects require a non-empty ID");
    }
    for (const GameplayModifier& modifier : effect.modifiers) {
        if (!state.baseValues.contains(modifier.attribute)) {
            throw std::invalid_argument(
                "Unknown Gameplay Effect numeric attribute: " +
                modifier.attribute);
        }
        if (modifier.operation != "Add" && modifier.operation != "Multiply" &&
            modifier.operation != "Override") {
            throw std::invalid_argument(
                "Unsupported Gameplay Effect modifier operation: " +
                modifier.operation);
        }
        static_cast<void>(resolveMagnitude(modifier, spec, spec->stacks));
        if (modifier.minimum.has_value()) {
            static_cast<void>(unrestrictedNumeric(
                *modifier.minimum, "Gameplay Effect modifier minimum"));
        }
    }
    if (std::any_of(effect.grantedTags.begin(), effect.grantedTags.end(),
                    [](const std::string& tag) {
                        return tag.empty();
                    })) {
        throw std::invalid_argument(
            "Granted Gameplay Tag must be a non-empty string");
    }
    for (const std::shared_ptr<GameplayAbility>& ability :
         effect.grantedAbilities) {
        validateAbility(state, ability);
    }
    if (effect.durationPolicy == "Instant" && !effect.grantedTags.empty()) {
        throw std::invalid_argument(
            "Instant Gameplay Effects cannot grant tags");
    }
    if (effect.durationPolicy == "Instant" &&
        !effect.grantedAbilities.empty()) {
        throw std::invalid_argument(
            "Instant Gameplay Effects cannot grant abilities");
    }
}

std::shared_ptr<ActiveGameplayEffect> findStackableEffect(
    const AbilitySystemImpl& state,
    const std::shared_ptr<GameplayEffectSpec>& spec) {
    if (spec->effect->id.empty()) {
        return nullptr;
    }
    for (const int handle : state.activeEffectOrder) {
        const std::shared_ptr<ActiveGameplayEffect> active =
            activeEffect(state, handle);
        if (active != nullptr && active->spec->effect->id == spec->effect->id &&
            runtimeEqual(active->spec->sourceKey, spec->sourceKey)) {
            return active;
        }
    }
    return nullptr;
}

std::pair<GameplayNumbers, bool> instantBases(
    const AbilitySystemImpl& state,
    const std::shared_ptr<GameplayEffectSpec>& spec) {
    GameplayNumbers result = state.baseValues;
    bool changed = false;
    for (const GameplayModifier& modifier : spec->effect->modifiers) {
        NumericValue base = validateNumeric(state, modifier.attribute,
                                            result.at(modifier.attribute),
                                            "Numeric attribute base");
        NumericValue magnitude = resolveMagnitude(modifier, spec, spec->stacks);
        if (spec->effect->stackingPolicy == "Aggregate") {
            magnitude =
                scaleMagnitude(magnitude, modifier.operation, spec->stacks);
        }
        if (modifier.operation == "Add") {
            base = addNumbers(base, magnitude);
        } else if (modifier.operation == "Multiply") {
            base = multiplyNumbers(base, magnitude);
        } else if (modifier.operation == "Override") {
            base = magnitude;
        }
        if (modifier.minimum.has_value()) {
            const NumericValue minimum = unrestrictedNumeric(
                *modifier.minimum, "Gameplay Effect modifier minimum");
            if (base.value < minimum.value) {
                base = minimum;
            }
        }
        const GameplayNumber value = resolvedNumber(base);
        static_cast<void>(validateNumeric(state, modifier.attribute, value,
                                          "Gameplay Effect result"));
        result[modifier.attribute] = value;
        changed = true;
    }
    return {std::move(result), changed};
}

void changeTagCount(AbilitySystemImpl& state, const std::string& tag,
                    int delta) {
    if (tag.empty()) {
        throw std::invalid_argument("Gameplay Tag must not be empty");
    }
    const int count = state.tagCounts[tag] + delta;
    if (count < 0) {
        throw std::logic_error("Gameplay Tag count cannot be negative: " + tag);
    }
    if (count == 0) {
        state.tagCounts.erase(tag);
    } else {
        state.tagCounts[tag] = count;
    }
}

bool validateGameplayEffectSpec(
    const AbilitySystemImpl& state,
    const std::shared_ptr<GameplayEffectSpec>& spec) {
    validateEffectSpec(state, spec);
    if (spec->effect->durationPolicy == "Instant") {
        auto [bases, changed] = instantBases(state, spec);
        if (changed) {
            static_cast<void>(preview(state, bases));
        }
        return true;
    }
    const std::shared_ptr<ActiveGameplayEffect> existing =
        findStackableEffect(state, spec);
    if (existing == nullptr) {
        static_cast<void>(preview(state, state.baseValues, spec));
    } else if (spec->effect->stackingPolicy == "Aggregate") {
        static_cast<void>(preview(state, state.baseValues, {}, existing->handle,
                                  existing->stacks + spec->stacks));
    }
    return true;
}

std::optional<int> applyGameplayEffectSpec(
    AbilitySystemImpl& state, const std::shared_ptr<GameplayEffectSpec>& spec) {
    validateEffectSpec(state, spec);
    if (spec->effect->durationPolicy == "Instant") {
        auto [bases, changed] = instantBases(state, spec);
        if (changed) {
            commitBases(state, bases);
        }
        return std::nullopt;
    }

    const std::shared_ptr<ActiveGameplayEffect> existing =
        findStackableEffect(state, spec);
    if (existing != nullptr) {
        if (spec->effect->stackingPolicy == "Aggregate") {
            const int replacementStacks = existing->stacks + spec->stacks;
            const GameplayNumbers current =
                preview(state, state.baseValues, {}, existing->handle,
                        replacementStacks);
            existing->stacks = replacementStacks;
            applyCurrentValues(
                state, current,
                AbilitySystemImpl::AttributeChangeSource::Effect);
            ++state.revision;
        }
        return existing->handle;
    }

    const GameplayNumbers current = preview(state, state.baseValues, spec);
    const int handle = state.nextEffectHandle++;
    std::shared_ptr<ActiveGameplayEffect> active =
        std::make_shared<ActiveGameplayEffect>(handle, spec,
                                               state.nextEffectOrder++);
    state.activeEffects.emplace(handle, active);
    state.activeEffectOrder.push_back(handle);
    for (const std::string& tag : spec->effect->grantedTags) {
        changeTagCount(state, tag, 1);
    }
    for (const std::shared_ptr<GameplayAbility>& ability :
         spec->effect->grantedAbilities) {
        active->grantedAbilitySpecs.push_back(
            giveAbility(state, ability, runtimeObject(active)));
    }
    applyCurrentValues(state, current,
                       AbilitySystemImpl::AttributeChangeSource::Effect);
    ++state.revision;
    return handle;
}

bool removeActiveGameplayEffect(AbilitySystemImpl& state, int handle,
                                std::optional<int> stacks) {
    const std::shared_ptr<ActiveGameplayEffect> active =
        activeEffect(state, handle);
    if (active == nullptr) {
        return false;
    }
    const int removeStacks = stacks.value_or(active->stacks);
    if (removeStacks <= 0) {
        throw std::invalid_argument("Removed stacks must be positive");
    }
    if (active->spec->effect->stackingPolicy == "Aggregate" &&
        removeStacks < active->stacks) {
        const int replacementStacks = active->stacks - removeStacks;
        const GameplayNumbers current = preview(
            state, state.baseValues, {}, active->handle, replacementStacks);
        active->stacks = replacementStacks;
        applyCurrentValues(state, current,
                           AbilitySystemImpl::AttributeChangeSource::Effect);
        ++state.revision;
        return true;
    }

    const GameplayNumbers current =
        preview(state, state.baseValues, {}, active->handle, 0);
    state.activeEffects.erase(handle);
    std::erase(state.activeEffectOrder, handle);
    for (const std::string& tag : active->spec->effect->grantedTags) {
        changeTagCount(state, tag, -1);
    }
    removeAbilitiesBySource(state, runtimeObject(active));
    applyCurrentValues(state, current,
                       AbilitySystemImpl::AttributeChangeSource::Effect);
    ++state.revision;
    return true;
}

int getActiveEffectStacks(const AbilitySystemImpl& state,
                          const std::string& effectID) {
    int result = 0;
    for (const int handle : state.activeEffectOrder) {
        const std::shared_ptr<ActiveGameplayEffect> active =
            activeEffect(state, handle);
        if (active != nullptr && active->spec->effect->id == effectID) {
            result += active->stacks;
        }
    }
    return result;
}

std::vector<std::shared_ptr<ActiveGameplayEffect>> getActiveGameplayEffects(
    const AbilitySystemImpl& state) {
    std::vector<std::shared_ptr<ActiveGameplayEffect>> result;
    result.reserve(state.activeEffectOrder.size());
    for (const int handle : state.activeEffectOrder) {
        const std::shared_ptr<ActiveGameplayEffect> active =
            activeEffect(state, handle);
        if (active != nullptr) {
            result.push_back(active);
        }
    }
    return result;
}

}  // namespace ludork::global::ability_system_impl
