#include "AttributeImpl.hpp"
#include "EffectImpl.hpp"
#include <Gameplay/GameplayEffect.hpp>
#include <stdexcept>
#include <utility>

namespace ludork::global::ability_system_impl {

namespace {
RuntimeValue attributeChangeValue(
    const AbilitySystemImpl::AttributeChange& change) {
    std::string source;
    switch (change.source) {
        case AbilitySystemImpl::AttributeChangeSource::Base:
            source = "Base";
            break;
        case AbilitySystemImpl::AttributeChangeSource::Effect:
            source = "Effect";
            break;
        case AbilitySystemImpl::AttributeChangeSource::Constraint:
            source = "Constraint";
            break;
    }
    RuntimeValue::Map value{{"source", RuntimeValue(std::move(source))},
                            {"force", RuntimeValue(change.force)}};
    if (change.oldBase.has_value()) {
        value.emplace("oldBase", runtimeNumber(*change.oldBase));
    }
    if (change.newBase.has_value()) {
        value.emplace("newBase", runtimeNumber(*change.newBase));
    }
    return RuntimeValue(std::move(value));
}

void accumulateModifiers(AbilitySystemImpl::ModifierAggregate& aggregate,
                         const std::string& attribute,
                         const std::shared_ptr<GameplayEffectSpec>& spec,
                         int stacks) {
    for (const GameplayModifier& modifier : spec->effect->modifiers) {
        if (modifier.attribute != attribute) {
            continue;
        }
        NumericValue magnitude = resolveMagnitude(modifier, spec, stacks);
        if (spec->effect->stackingPolicy == "Aggregate") {
            magnitude = scaleMagnitude(magnitude, modifier.operation, stacks);
        }
        if (modifier.operation == "Add") {
            aggregate.additive = addNumbers(aggregate.additive, magnitude);
        } else if (modifier.operation == "Multiply") {
            aggregate.multiplier =
                multiplyNumbers(aggregate.multiplier, magnitude);
        } else if (modifier.operation == "Override") {
            aggregate.overrideValue = magnitude;
        } else {
            throw std::invalid_argument(
                "Unsupported Gameplay Effect modifier operation: " +
                modifier.operation);
        }
        if (modifier.minimum.has_value()) {
            const NumericValue minimum = unrestrictedNumeric(
                *modifier.minimum, "Gameplay Effect modifier minimum");
            if (!aggregate.minimum.has_value() ||
                minimum.value > aggregate.minimum->value) {
                aggregate.minimum = minimum;
            }
        }
    }
}

}  // namespace

void initialize(AbilitySystemImpl& state,
                std::shared_ptr<AttributeSet> attributes) {
    state.attributeSet = std::move(attributes);
    if (state.attributeSet == nullptr) {
        throw std::invalid_argument("Ability System requires an AttributeSet");
    }
    for (const std::string& name : state.attributeSet->getAttributeNames()) {
        const std::optional<AttributeSet::NumericType> type =
            state.attributeSet->getNumericAttributeType(name);
        if (!type.has_value()) {
            continue;
        }
        GameplayNumber value =
            state.attributeSet->getNumericAttributeValue(name);
        static_cast<void>(
            numericValue(value, *type, "Numeric attribute default", name));
        state.numericAttributes.push_back(name);
        state.baseValues.emplace(name, value);
    }
}

void requireNumericAttribute(const AbilitySystemImpl& state,
                             const std::string& name) {
    if (!state.baseValues.contains(name)) {
        throw std::invalid_argument("Unknown numeric attribute: " + name);
    }
}

NumericValue resolveAttribute(
    const AbilitySystemImpl& state, const std::string& name,
    const GameplayNumbers& bases,
    const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
    std::optional<int> replacedHandle, std::optional<int> replacementStacks,
    const GameplayNumbers& resolvedValues) {
    AbilitySystemImpl::ModifierAggregate aggregate;
    for (const int handle : state.activeEffectOrder) {
        const std::shared_ptr<ActiveGameplayEffect> active =
            activeEffect(state, handle);
        if (active == nullptr) {
            continue;
        }
        const int stacks =
            replacedHandle.has_value() && handle == *replacedHandle
                ? replacementStacks.value_or(active->stacks)
                : active->stacks;
        if (stacks > 0) {
            accumulateModifiers(aggregate, name, active->spec, stacks);
        }
    }
    if (pendingSpec != nullptr) {
        accumulateModifiers(aggregate, name, pendingSpec, pendingSpec->stacks);
    }

    const auto base = bases.find(name);
    if (base == bases.end()) {
        throw std::logic_error("Numeric base is missing: " + name);
    }
    NumericValue current =
        validateNumeric(state, name, base->second, "Numeric attribute base");
    current = multiplyNumbers(addNumbers(current, aggregate.additive),
                              aggregate.multiplier);
    if (aggregate.overrideValue.has_value()) {
        current = *aggregate.overrideValue;
    }
    if (aggregate.minimum.has_value() &&
        current.value < aggregate.minimum->value) {
        current = *aggregate.minimum;
    }

    const std::optional<GameplayNumber> constrained =
        state.constrain(name, resolvedNumber(current), resolvedValues);
    if (constrained.has_value()) {
        current = validateNumeric(state, name, *constrained,
                                  "Numeric attribute current value");
    } else {
        current = validateNumeric(state, name, resolvedNumber(current),
                                  "Numeric attribute current value");
    }
    return current;
}

GameplayNumbers preview(const AbilitySystemImpl& state,
                        const GameplayNumbers& bases,
                        const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
                        std::optional<int> replacedHandle,
                        std::optional<int> replacementStacks) {
    GameplayNumbers values;
    for (const std::string& name : state.numericAttributes) {
        if (name != "HP") {
            values.emplace(name,
                           resolvedNumber(resolveAttribute(
                               state, name, bases, pendingSpec, replacedHandle,
                               replacementStacks, values)));
        }
    }
    if (bases.contains("HP")) {
        values.emplace("HP", resolvedNumber(resolveAttribute(
                                 state, "HP", bases, pendingSpec,
                                 replacedHandle, replacementStacks, values)));
    }
    return values;
}

void notify(const AbilitySystemImpl& state, const std::string& name,
            const GameplayNumber& oldValue, const GameplayNumber& newValue,
            const AbilitySystemImpl::AttributeChange& change) {
    if (state.suppressAttributeListeners ||
        (runtimeEqual(oldValue, newValue) && !change.force)) {
        return;
    }
    const auto entries = state.listeners.find(name);
    if (entries == state.listeners.end()) {
        return;
    }
    for (const AbilitySystemImpl::Listener& listener : entries->second) {
        RuntimeValue::Array arguments{runtimeNumber(oldValue),
                                      runtimeNumber(newValue),
                                      attributeChangeValue(change)};
        arguments.insert(arguments.end(), listener.params.begin(),
                         listener.params.end());
        static_cast<void>(
            invokeCallable(listener.callback, std::move(arguments)));
    }
}

AppliedCurrentValues applyCurrentValues(
    AbilitySystemImpl& state, const GameplayNumbers& values,
    AbilitySystemImpl::AttributeChangeSource source,
    const GameplayNumbers* oldBases, const GameplayNumbers* newBases,
    const GameplayNumbers* oldValueOverrides) {
    GameplayNumbers oldValues;
    for (const std::string& name : state.numericAttributes) {
        const auto overrideValue = oldValueOverrides == nullptr
                                       ? GameplayNumbers::const_iterator{}
                                       : oldValueOverrides->find(name);
        if (oldValueOverrides != nullptr &&
            overrideValue != oldValueOverrides->end()) {
            oldValues.emplace(name, overrideValue->second);
        } else {
            oldValues.emplace(
                name, state.attributeSet->getNumericAttributeValue(name));
        }
    }

    state.suppressAttributeListeners = true;
    state.internalAttributeWrite = true;
    try {
        for (const std::string& name : state.numericAttributes) {
            const auto value = values.find(name);
            if (value == values.end()) {
                throw std::logic_error(
                    "Resolved numeric attribute is missing: " + name);
            }
            static_cast<void>(validateNumeric(
                state, name, value->second, "Numeric attribute current value"));
            state.attributeSet->setNumericAttributeValue(name, value->second);
        }
    } catch (...) {
        state.internalAttributeWrite = false;
        state.suppressAttributeListeners = false;
        throw;
    }
    state.internalAttributeWrite = false;
    state.suppressAttributeListeners = false;

    AppliedCurrentValues applied;
    for (const std::string& name : state.numericAttributes) {
        AbilitySystemImpl::AttributeChange change{source};
        if (source == AbilitySystemImpl::AttributeChangeSource::Base) {
            change.oldBase = oldBases->at(name);
            change.newBase = newBases->at(name);
            change.force = !runtimeEqual(*change.oldBase, *change.newBase);
        }
        const GameplayNumber current =
            state.attributeSet->getNumericAttributeValue(name);
        const bool fieldChanged =
            !runtimeEqual(oldValues.at(name), current) || change.force;
        applied.changed = applied.changed || fieldChanged;
        applied.pending.push_back(
            {name, oldValues.at(name), current, std::move(change)});
    }
    return applied;
}

void flushAppliedCurrentValues(AbilitySystemImpl& state,
                               AppliedCurrentValues applied,
                               bool bumpRevision) {
    if (bumpRevision) {
        ++state.revision;
    }
    for (const AppliedCurrentValues::Notification& notification :
         applied.pending) {
        notify(state, notification.name, notification.oldValue,
               notification.newValue, notification.change);
    }
}

void commitBases(AbilitySystemImpl& state, const GameplayNumbers& bases,
                 const GameplayNumbers* oldValueOverrides) {
    const GameplayNumbers oldBases = state.baseValues;
    const GameplayNumbers currentValues = preview(state, bases);
    bool baseChanged = false;
    for (const std::string& name : state.numericAttributes) {
        if (!runtimeEqual(oldBases.at(name), bases.at(name))) {
            baseChanged = true;
            break;
        }
    }
    state.baseValues = bases;
    AppliedCurrentValues applied = applyCurrentValues(
        state, currentValues, AbilitySystemImpl::AttributeChangeSource::Base,
        &oldBases, &state.baseValues, oldValueOverrides);
    const bool bumpRevision = baseChanged || applied.changed;
    flushAppliedCurrentValues(state, std::move(applied), bumpRevision);
}

std::shared_ptr<AttributeSet> getAttributeSet(const AbilitySystemImpl& state) {
    return state.attributeSet;
}

GameplayNumber getNumericAttribute(const AbilitySystemImpl& state,
                                   const std::string& name) {
    requireNumericAttribute(state, name);
    return state.attributeSet->getNumericAttributeValue(name);
}

GameplayNumber getNumericAttributeBase(const AbilitySystemImpl& state,
                                       const std::string& name) {
    requireNumericAttribute(state, name);
    return state.baseValues.at(name);
}

void setNumericAttributeBase(AbilitySystemImpl& state, const std::string& name,
                             const GameplayNumber& value) {
    requireNumericAttribute(state, name);
    static_cast<void>(
        validateNumeric(state, name, value, "Numeric attribute base"));
    GameplayNumbers bases = state.baseValues;
    bases[name] = value;
    commitBases(state, bases);
}

void setNumericAttributeBases(AbilitySystemImpl& state,
                              const GameplayNumbers& values) {
    GameplayNumbers bases = state.baseValues;
    for (const auto& [name, value] : values) {
        requireNumericAttribute(state, name);
        static_cast<void>(
            validateNumeric(state, name, value, "Numeric attribute base"));
        bases[name] = value;
    }
    commitBases(state, bases);
}

GameplayNumbers getNumericAttributeBases(const AbilitySystemImpl& state) {
    return state.baseValues;
}

void addAttributeChangeListener(AbilitySystemImpl& state,
                                const std::string& name,
                                RuntimeIdentityPtr callback,
                                RuntimeValue::Array params) {
    requireNumericAttribute(state, name);
    if (callback == nullptr) {
        throw std::invalid_argument(
            "Attribute change listener must be a function");
    }
    state.listeners[name].push_back(
        {RuntimeHandle(std::move(callback)), std::move(params)});
}

void refreshConstraints(AbilitySystemImpl& state) {
    const GameplayNumbers current = preview(state, state.baseValues);
    AppliedCurrentValues applied = applyCurrentValues(
        state, current, AbilitySystemImpl::AttributeChangeSource::Constraint);
    const bool bumpRevision = applied.changed;
    flushAppliedCurrentValues(state, std::move(applied), bumpRevision);
}

int getRevision(const AbilitySystemImpl& state) {
    return state.revision;
}

void onAttributeWrite(AbilitySystemImpl& state, const std::string& name,
                      const GameplayNumber& oldValue,
                      const GameplayNumber& newValue) {
    GameplayNumbers bases = state.baseValues;
    bases[name] = newValue;
    const GameplayNumbers overrides{{name, oldValue}};
    commitBases(state, bases, &overrides);
}

}  // namespace ludork::global::ability_system_impl
