#pragma once
#include "AbilitySystemImpl.hpp"
#include <Gameplay/GameplayEventData.hpp>

namespace ludork::global::ability_system_impl {

void initialize(AbilitySystemImpl& state,
                std::shared_ptr<AttributeSet> attributes);

void requireNumericAttribute(const AbilitySystemImpl& state,
                             const std::string& name);

template <typename Value>
NumericValue validateNumeric(const AbilitySystemImpl& state,
                             const std::string& name, const Value& value,
                             const std::string& context) {
    return numericValue(value, state.attributeSet->getAttributeType(name),
                        context, name);
}

NumericValue resolveAttribute(
    const AbilitySystemImpl& state, const std::string& name,
    const AttributeNumbers& bases,
    const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
    std::optional<int> replacedHandle, std::optional<int> replacementStacks,
    const AttributeNumbers& resolvedValues);

AttributeNumbers preview(
    const AbilitySystemImpl& state, const AttributeNumbers& bases,
    const std::shared_ptr<GameplayEffectSpec>& pendingSpec = {},
    std::optional<int> replacedHandle = std::nullopt,
    std::optional<int> replacementStacks = std::nullopt);

void notify(const AbilitySystemImpl& state, const std::string& name,
            const RuntimeValue& oldValue, const RuntimeValue& newValue,
            const RuntimeValue::Map& change);

bool applyCurrentValues(AbilitySystemImpl& state,
                        const AttributeNumbers& values,
                        const std::string& source,
                        const AttributeNumbers* oldBases = nullptr,
                        const AttributeNumbers* newBases = nullptr,
                        const RuntimeValue::Map* oldValueOverrides = nullptr);

void commitBases(AbilitySystemImpl& state, const AttributeNumbers& bases,
                 const RuntimeValue::Map* oldValueOverrides = nullptr);

std::shared_ptr<AttributeSet> getAttributeSet(const AbilitySystemImpl& state);

RuntimeValue getNumericAttribute(const AbilitySystemImpl& state,
                                 const std::string& name);

RuntimeValue getNumericAttributeBase(const AbilitySystemImpl& state,
                                     const std::string& name);

void setNumericAttributeBase(AbilitySystemImpl& state, const std::string& name,
                             const RuntimeValue& value);

void setNumericAttributeBases(AbilitySystemImpl& state,
                              const RuntimeValue::Map& values);

RuntimeValue::Map getNumericAttributeBases(const AbilitySystemImpl& state);

void addAttributeChangeListener(AbilitySystemImpl& state,
                                const std::string& name,
                                RuntimeIdentityPtr callback,
                                RuntimeValue::Array params = {});

void setNumericAttributeConstraint(AbilitySystemImpl& state,
                                   const std::string& name,
                                   RuntimeIdentityPtr callback);

int getRevision(const AbilitySystemImpl& state);

void onAttributeWrite(AbilitySystemImpl& state, const std::string& name,
                      const RuntimeValue& oldValue,
                      const RuntimeValue& newValue);
}  // namespace ludork::global::ability_system_impl
