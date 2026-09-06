#include "AbilityImpl.hpp"
#include <Gameplay/GameplayAbility.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ludork::global::ability_system_impl {

bool tagMatches(const std::string& tag, const std::string& query) {
    return tag == query || (tag.size() > query.size() &&
                            tag.starts_with(query) && tag[query.size()] == '.');
}

void validateAbility(const AbilitySystemImpl& state,
                     const std::shared_ptr<GameplayAbility>& ability) {
    if (ability == nullptr) {
        throw std::invalid_argument(
            "Ability System can only grant GameplayAbility instances");
    }
    if (ability->id.empty()) {
        throw std::invalid_argument(
            "Gameplay Ability ID must be a non-empty string");
    }
    if (!std::isfinite(ability->priority)) {
        throw std::invalid_argument("Gameplay Ability priority must be finite");
    }
    for (const std::vector<std::string>* tags :
         {&ability->abilityTags, &ability->requiredTags, &ability->blockedTags,
          &ability->triggerTags}) {
        if (std::any_of(tags->begin(), tags->end(), [](const std::string& tag) {
                return tag.empty();
            })) {
            throw std::invalid_argument(
                "Gameplay Ability Tags must be non-empty strings");
        }
    }
}

std::shared_ptr<GameplayAbilitySpec> giveAbility(
    AbilitySystemImpl& state, std::shared_ptr<GameplayAbility> ability,
    RuntimeValue sourceKey) {
    validateAbility(state, ability);
    std::shared_ptr<GameplayAbilitySpec> spec =
        std::make_shared<GameplayAbilitySpec>(
            std::move(ability), std::move(sourceKey), state.nextAbilityOrder++);
    state.abilities.push_back(spec);
    ++state.revision;
    return spec;
}

void removeAbilitiesBySource(AbilitySystemImpl& state,
                             const RuntimeValue& sourceKey) {
    const std::size_t oldSize = state.abilities.size();
    std::erase_if(state.abilities,
                  [&](const std::shared_ptr<GameplayAbilitySpec>& spec) {
                      return runtimeEqual(spec->sourceKey, sourceKey);
                  });
    if (state.abilities.size() != oldSize) {
        ++state.revision;
    }
}

bool hasMatchingGameplayTag(const AbilitySystemImpl& state,
                            const std::string& tag) {
    return std::any_of(
        state.tagCounts.begin(), state.tagCounts.end(), [&](const auto& entry) {
            return entry.second > 0 && tagMatches(entry.first, tag);
        });
}

std::vector<std::shared_ptr<GameplayAbilitySpec>> abilityCandidates(
    const AbilitySystemImpl& state, const std::string& abilityID) {
    std::vector<std::shared_ptr<GameplayAbilitySpec>> matches;
    for (const std::shared_ptr<GameplayAbilitySpec>& spec : state.abilities) {
        if (spec->ability->id == abilityID) {
            matches.push_back(spec);
        }
    }
    std::sort(matches.begin(), matches.end(),
              [](const auto& left, const auto& right) {
                  if (left->ability->priority != right->ability->priority) {
                      return left->ability->priority > right->ability->priority;
                  }
                  return left->grantOrder < right->grantOrder;
              });
    return matches;
}

std::vector<std::shared_ptr<GameplayAbilitySpec>> eventCandidates(
    const AbilitySystemImpl& state, const GameplayEventData& eventData) {
    std::vector<std::shared_ptr<GameplayAbilitySpec>> matches;
    for (const std::shared_ptr<GameplayAbilitySpec>& spec : state.abilities) {
        if (std::any_of(spec->ability->triggerTags.begin(),
                        spec->ability->triggerTags.end(),
                        [&](const std::string& tag) {
                            return tagMatches(eventData.eventTag, tag);
                        })) {
            matches.push_back(spec);
        }
    }
    std::sort(matches.begin(), matches.end(),
              [](const auto& left, const auto& right) {
                  if (left->ability->priority != right->ability->priority) {
                      return left->ability->priority > right->ability->priority;
                  }
                  return left->grantOrder < right->grantOrder;
              });
    return matches;
}

}  // namespace ludork::global::ability_system_impl
