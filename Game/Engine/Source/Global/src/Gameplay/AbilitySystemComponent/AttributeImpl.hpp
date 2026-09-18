#pragma once
#include "AbilitySystemImpl.hpp"
#include <Gameplay/GameplayEventData.hpp>
#include <vector>

namespace ludork::global::ability_system_impl {

void initialize(AbilitySystemImpl& state,
                std::shared_ptr<AttributeSet> attributes);

void requireNumericAttribute(const AbilitySystemImpl& state,
                             const std::string& name);

template <typename Value>
NumericValue validateNumeric(const AbilitySystemImpl& state,
                             const std::string& name, const Value& value,
                             const std::string& context) {
    const std::optional<AttributeSet::NumericType> type =
        state.attributeSet->getNumericAttributeType(name);
    if (!type.has_value()) {
        throw std::invalid_argument("Attribute is not numeric: " + name);
    }
    return numericValue(value, *type, context, name);
}

NumericValue resolveAttribute(
    const AbilitySystemImpl& state, const std::string& name,
    const GameplayNumbers& bases,
    const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
    std::optional<int> replacedHandle, std::optional<int> replacementStacks,
    const GameplayNumbers& resolvedValues);

GameplayNumbers preview(
    const AbilitySystemImpl& state, const GameplayNumbers& bases,
    const std::shared_ptr<GameplayEffectSpec>& pendingSpec = {},
    std::optional<int> replacedHandle = std::nullopt,
    std::optional<int> replacementStacks = std::nullopt);

void notify(const AbilitySystemImpl& state, const std::string& name,
            const RuntimeValue& oldValue, const RuntimeValue& newValue,
            const AbilitySystemImpl::AttributeChange& change);

struct AppliedCurrentValues {
    struct Notification {
        std::string name;
        RuntimeValue oldValue;
        RuntimeValue newValue;
        AbilitySystemImpl::AttributeChange change;
    };

    bool changed = false;
    std::vector<Notification> pending;
};

AppliedCurrentValues applyCurrentValues(
    AbilitySystemImpl& state, const GameplayNumbers& values,
    AbilitySystemImpl::AttributeChangeSource source,
    const GameplayNumbers* oldBases = nullptr,
    const GameplayNumbers* newBases = nullptr,
    const RuntimeValue::Map* oldValueOverrides = nullptr);

void flushAppliedCurrentValues(AbilitySystemImpl& state,
                               AppliedCurrentValues applied, bool bumpRevision);

void commitBases(AbilitySystemImpl& state, const GameplayNumbers& bases,
                 const RuntimeValue::Map* oldValueOverrides = nullptr);

std::shared_ptr<AttributeSet> getAttributeSet(const AbilitySystemImpl& state);

GameplayNumber getNumericAttribute(const AbilitySystemImpl& state,
                                   const std::string& name);

GameplayNumber getNumericAttributeBase(const AbilitySystemImpl& state,
                                       const std::string& name);

void setNumericAttributeBase(AbilitySystemImpl& state, const std::string& name,
                             const GameplayNumber& value);

void setNumericAttributeBases(AbilitySystemImpl& state,
                              const GameplayNumbers& values);

GameplayNumbers getNumericAttributeBases(const AbilitySystemImpl& state);

void addAttributeChangeListener(AbilitySystemImpl& state,
                                const std::string& name,
                                RuntimeIdentityPtr callback,
                                RuntimeValue::Array params = {});

void refreshConstraints(AbilitySystemImpl& state);

int getRevision(const AbilitySystemImpl& state);

void onAttributeWrite(AbilitySystemImpl& state, const std::string& name,
                      const RuntimeValue& oldValue,
                      const RuntimeValue& newValue);
}  // namespace ludork::global::ability_system_impl
