#pragma once
#include <Gameplay/AbilitySystemComponent.hpp>
#include "AbilitySystemComponent/AbilitySystemImpl.hpp"

struct AbilitySystemComponent::Impl {
    AbilitySystemComponent* component;
    RuntimeValue owner;
    std::unordered_map<std::string, NumericConstraint> constraints;
    ludork::global::ability_system_impl::AbilitySystemImpl state;

    Impl(AbilitySystemComponent* ownerComponent, RuntimeValue systemOwner,
         std::shared_ptr<AttributeSet> attributes);
    std::shared_ptr<AbilitySystemComponent> self() const;
};
