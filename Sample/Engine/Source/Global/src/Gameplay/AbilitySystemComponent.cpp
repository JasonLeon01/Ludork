#include <Gameplay/AbilitySystemComponent.hpp>
#include "AbilitySystemComponentImpl.hpp"
#include "AbilitySystemComponent/AbilityImpl.hpp"
#include "AbilitySystemComponent/AttributeImpl.hpp"
#include "AbilitySystemComponent/EffectImpl.hpp"
#include <Gameplay/GameplayAbility.hpp>
#include <Gameplay/GameplayAbilityResult.hpp>
#include <Gameplay/GameplayEventData.hpp>

#include <stdexcept>
#include <utility>

AbilitySystemComponent::Impl::Impl(AbilitySystemComponent* ownerComponent,
                                   RuntimeValue systemOwner,
                                   std::shared_ptr<AttributeSet> attributes)
    : component(ownerComponent), owner(std::move(systemOwner)) {
    state.selfValue = [this]() {
        return selfValue();
    };
    ludork::global::ability_system_impl::initialize(state,
                                                    std::move(attributes));
}

RuntimeValue AbilitySystemComponent::Impl::selfValue() const {
    std::shared_ptr<RuntimeObject> ownerObject = component->runtimeOwner();
    if (ownerObject == nullptr) {
        ownerObject = component->weak_from_this().lock();
    }
    if (ownerObject == nullptr) {
        throw std::logic_error("Ability System has no stable runtime owner");
    }
    return RuntimeValue(ownerObject);
}

std::shared_ptr<AbilitySystemComponent> AbilitySystemComponent::Impl::self()
    const {
    std::shared_ptr<RuntimeObject> ownerObject =
        component->weak_from_this().lock();
    if (ownerObject == nullptr) {
        throw std::logic_error("Ability System has no stable runtime owner");
    }
    return std::shared_ptr<AbilitySystemComponent>(std::move(ownerObject),
                                                   component);
}

AbilitySystemComponent::AbilitySystemComponent(
    RuntimeValue owner, std::shared_ptr<AttributeSet> attributeSet)
    : impl_(std::make_unique<Impl>(this, std::move(owner),
                                   std::move(attributeSet))) {}

AbilitySystemComponent::~AbilitySystemComponent() = default;

RuntimeValue AbilitySystemComponent::getOwner() const {
    return impl_->owner;
}

std::shared_ptr<AttributeSet> AbilitySystemComponent::getAttributeSet() const {
    return ludork::global::ability_system_impl::getAttributeSet(impl_->state);
}

RuntimeValue AbilitySystemComponent::getNumericAttribute(
    const std::string& name) const {
    return ludork::global::ability_system_impl::getNumericAttribute(
        impl_->state, name);
}

RuntimeValue AbilitySystemComponent::getNumericAttributeBase(
    const std::string& name) const {
    return ludork::global::ability_system_impl::getNumericAttributeBase(
        impl_->state, name);
}

void AbilitySystemComponent::setNumericAttributeBase(
    const std::string& name, const RuntimeValue& value) {
    ludork::global::ability_system_impl::setNumericAttributeBase(impl_->state,
                                                                 name, value);
}

void AbilitySystemComponent::setNumericAttributeBases(
    const RuntimeValue::Map& values) {
    ludork::global::ability_system_impl::setNumericAttributeBases(impl_->state,
                                                                  values);
}

RuntimeValue::Map AbilitySystemComponent::getNumericAttributeBases() const {
    return ludork::global::ability_system_impl::getNumericAttributeBases(
        impl_->state);
}

void AbilitySystemComponent::addAttributeChangeListener(
    const std::string& name, RuntimeIdentityPtr callback,
    RuntimeValue::Array params) {
    ludork::global::ability_system_impl::addAttributeChangeListener(
        impl_->state, name, std::move(callback), std::move(params));
}

void AbilitySystemComponent::setNumericAttributeConstraint(
    const std::string& name, RuntimeIdentityPtr callback) {
    ludork::global::ability_system_impl::setNumericAttributeConstraint(
        impl_->state, name, std::move(callback));
}

std::shared_ptr<GameplayAbilitySpec> AbilitySystemComponent::giveAbility(
    std::shared_ptr<GameplayAbility> ability, RuntimeValue sourceKey) {
    return ludork::global::ability_system_impl::giveAbility(
        impl_->state, std::move(ability), std::move(sourceKey));
}

void AbilitySystemComponent::removeAbilitiesBySource(
    const RuntimeValue& sourceKey) {
    ludork::global::ability_system_impl::removeAbilitiesBySource(impl_->state,
                                                                 sourceKey);
}

std::shared_ptr<GameplayAbilityResult>
AbilitySystemComponent::tryActivateAbility(
    const std::string& abilityID,
    std::shared_ptr<GameplayEventData> eventData) {
    if (eventData == nullptr) {
        eventData = std::make_shared<GameplayEventData>();
    }
    const std::vector<std::shared_ptr<GameplayAbilitySpec>> matches =
        ludork::global::ability_system_impl::abilityCandidates(impl_->state,
                                                               abilityID);
    if (matches.empty()) {
        return GameplayAbilityResult::Failure(
            RuntimeValue("AbilityNotFound"),
            ludork::global::ability_system_impl::runtimeMap(
                {{"abilityID", RuntimeValue(abilityID)}}));
    }
    const std::shared_ptr<AbilitySystemComponent> self = impl_->self();
    for (const std::shared_ptr<GameplayAbilitySpec>& spec : matches) {
        const std::shared_ptr<GameplayAbilityResult> gate =
            spec->ability->canActivate(self, eventData);
        if (gate == nullptr) {
            throw std::invalid_argument(
                "Gameplay Ability gate must return GameplayAbilityResult");
        }
        if (gate->ok) {
            const std::shared_ptr<GameplayAbilityResult> result =
                spec->ability->activate(self, eventData);
            if (result == nullptr) {
                throw std::invalid_argument(
                    "Gameplay Ability must return GameplayAbilityResult");
            }
            return result;
        }
    }
    return GameplayAbilityResult::Failure(
        RuntimeValue("AbilityNotActivated"),
        ludork::global::ability_system_impl::runtimeMap(
            {{"abilityID", RuntimeValue(abilityID)}}));
}

std::vector<std::shared_ptr<GameplayAbilityResult>>
AbilitySystemComponent::handleGameplayEvent(
    const std::shared_ptr<GameplayEventData>& eventData) {
    if (eventData == nullptr) {
        throw std::invalid_argument("Gameplay Event must be GameplayEventData");
    }
    if (eventData->eventTag.empty()) {
        throw std::invalid_argument("Gameplay Event Tag must not be empty");
    }
    const std::vector<std::shared_ptr<GameplayAbilitySpec>> matches =
        ludork::global::ability_system_impl::eventCandidates(impl_->state,
                                                             *eventData);
    const std::shared_ptr<AbilitySystemComponent> self = impl_->self();
    std::vector<std::shared_ptr<GameplayAbilityResult>> results;
    results.reserve(matches.size());
    for (const std::shared_ptr<GameplayAbilitySpec>& spec : matches) {
        std::shared_ptr<GameplayAbilityResult> result =
            spec->ability->canActivate(self, eventData);
        if (result == nullptr) {
            throw std::invalid_argument(
                "Gameplay Ability gate must return GameplayAbilityResult");
        }
        if (result->ok) {
            result = spec->ability->activate(self, eventData);
            if (result == nullptr) {
                throw std::invalid_argument(
                    "Gameplay Ability must return GameplayAbilityResult");
            }
        }
        results.push_back(std::move(result));
    }
    return results;
}

bool AbilitySystemComponent::validateGameplayEffectSpec(
    const std::shared_ptr<GameplayEffectSpec>& spec) const {
    return ludork::global::ability_system_impl::validateGameplayEffectSpec(
        impl_->state, spec);
}

std::optional<int> AbilitySystemComponent::applyGameplayEffectSpec(
    const std::shared_ptr<GameplayEffectSpec>& spec) {
    return ludork::global::ability_system_impl::applyGameplayEffectSpec(
        impl_->state, spec);
}

bool AbilitySystemComponent::removeActiveGameplayEffect(
    int handle, std::optional<int> stacks) {
    return ludork::global::ability_system_impl::removeActiveGameplayEffect(
        impl_->state, handle, stacks);
}

int AbilitySystemComponent::getActiveEffectStacks(
    const std::string& effectID) const {
    return ludork::global::ability_system_impl::getActiveEffectStacks(
        impl_->state, effectID);
}

std::vector<std::shared_ptr<ActiveGameplayEffect>>
AbilitySystemComponent::getActiveGameplayEffects() const {
    return ludork::global::ability_system_impl::getActiveGameplayEffects(
        impl_->state);
}

bool AbilitySystemComponent::hasMatchingGameplayTag(
    const std::string& tag) const {
    return ludork::global::ability_system_impl::hasMatchingGameplayTag(
        impl_->state, tag);
}

int AbilitySystemComponent::getRevision() const {
    return ludork::global::ability_system_impl::getRevision(impl_->state);
}

void AbilitySystemComponent::onAttributeWrite(const std::string& name,
                                              const RuntimeValue& oldValue,
                                              const RuntimeValue& newValue) {
    ludork::global::ability_system_impl::onAttributeWrite(impl_->state, name,
                                                          oldValue, newValue);
}
