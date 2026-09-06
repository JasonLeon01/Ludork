#include "AttributeImpl.hpp"
#include "EffectImpl.hpp"
#include <Gameplay/GameplayEffect.hpp>
#include <stdexcept>
#include <utility>

namespace ludork::global::ability_system_impl {

namespace {
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
        if (!modifier.minimum.isNil()) {
            const NumericValue minimum = unrestrictedNumeric(
                modifier.minimum, "Gameplay Effect modifier minimum");
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
        const std::string type = state.attributeSet->getAttributeType(name);
        if (type != "int" && type != "float") {
            continue;
        }
        RuntimeValue value = state.attributeSet->getAttributeValue(name);
        static_cast<void>(
            numericValue(value, type, "Numeric attribute default", name));
        state.numericAttributes.push_back(name);
        state.baseValues.emplace(name, attributeNumber(value));
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
    const AttributeNumbers& bases,
    const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
    std::optional<int> replacedHandle, std::optional<int> replacementStacks,
    const AttributeNumbers& resolvedValues) {
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

    const auto constraint = state.constraints.find(name);
    if (constraint != state.constraints.end()) {
        const std::vector<RuntimeValue> results = invokeCallable(
            constraint->second, {runtimeNumber(current), state.selfValue(),
                                 RuntimeValue(runtimeNumbers(resolvedValues))});
        if (results.size() != 1) {
            throw std::invalid_argument(
                "Numeric attribute constraint must return one value");
        }
        current = validateNumeric(state, name, results.front(),
                                  "Numeric attribute current value");
    } else {
        current = validateNumeric(state, name, runtimeNumber(current),
                                  "Numeric attribute current value");
    }
    return current;
}

AttributeNumbers preview(const AbilitySystemImpl& state,
                         const AttributeNumbers& bases,
                         const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
                         std::optional<int> replacedHandle,
                         std::optional<int> replacementStacks) {
    AttributeNumbers values;
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
            const RuntimeValue& oldValue, const RuntimeValue& newValue,
            const RuntimeValue::Map& change) {
    const bool force = change.contains("force") &&
                       change.at("force").getIf<bool>() != nullptr &&
                       *change.at("force").getIf<bool>();
    if (state.suppressAttributeListeners ||
        (runtimeEqual(oldValue, newValue) && !force)) {
        return;
    }
    const auto entries = state.listeners.find(name);
    if (entries == state.listeners.end()) {
        return;
    }
    for (const AbilitySystemImpl::Listener& listener : entries->second) {
        RuntimeValue::Array arguments{oldValue, newValue, RuntimeValue(change)};
        arguments.insert(arguments.end(), listener.params.begin(),
                         listener.params.end());
        static_cast<void>(
            invokeCallable(listener.callback, std::move(arguments)));
    }
}

bool applyCurrentValues(AbilitySystemImpl& state,
                        const AttributeNumbers& values,
                        const std::string& source,
                        const AttributeNumbers* oldBases,
                        const AttributeNumbers* newBases,
                        const RuntimeValue::Map* oldValueOverrides) {
    RuntimeValue::Map oldValues;
    for (const std::string& name : state.numericAttributes) {
        const auto overrideValue = oldValueOverrides == nullptr
                                       ? RuntimeValue::Map::const_iterator{}
                                       : oldValueOverrides->find(name);
        if (oldValueOverrides != nullptr &&
            overrideValue != oldValueOverrides->end()) {
            oldValues.emplace(name, overrideValue->second);
        } else {
            oldValues.emplace(name,
                              state.attributeSet->getAttributeValue(name));
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
            state.attributeSet->setAttributeValue(name,
                                                  runtimeNumber(value->second));
        }
    } catch (...) {
        state.internalAttributeWrite = false;
        state.suppressAttributeListeners = false;
        throw;
    }
    state.internalAttributeWrite = false;
    state.suppressAttributeListeners = false;

    bool changed = false;
    for (const std::string& name : state.numericAttributes) {
        RuntimeValue::Map change{{"source", RuntimeValue(source)},
                                 {"force", RuntimeValue(false)}};
        if (source == "Base") {
            const RuntimeValue oldBase = runtimeNumber(oldBases->at(name));
            const RuntimeValue newBase = runtimeNumber(newBases->at(name));
            const bool force = !runtimeEqual(oldBase, newBase);
            change["force"] = RuntimeValue(force);
            change["oldBase"] = oldBase;
            change["newBase"] = newBase;
        }
        const RuntimeValue current =
            state.attributeSet->getAttributeValue(name);
        const bool fieldChanged = !runtimeEqual(oldValues.at(name), current) ||
                                  *change.at("force").getIf<bool>();
        changed = changed || fieldChanged;
        notify(state, name, oldValues.at(name), current, change);
    }
    return changed;
}

void commitBases(AbilitySystemImpl& state, const AttributeNumbers& bases,
                 const RuntimeValue::Map* oldValueOverrides) {
    const AttributeNumbers oldBases = state.baseValues;
    const AttributeNumbers currentValues = preview(state, bases);
    bool baseChanged = false;
    for (const std::string& name : state.numericAttributes) {
        if (!runtimeEqual(oldBases.at(name), bases.at(name))) {
            baseChanged = true;
            break;
        }
    }
    state.baseValues = bases;
    const bool currentChanged =
        applyCurrentValues(state, currentValues, "Base", &oldBases,
                           &state.baseValues, oldValueOverrides);
    if (baseChanged || currentChanged) {
        ++state.revision;
    }
}

std::shared_ptr<AttributeSet> getAttributeSet(const AbilitySystemImpl& state) {
    return state.attributeSet;
}

RuntimeValue getNumericAttribute(const AbilitySystemImpl& state,
                                 const std::string& name) {
    requireNumericAttribute(state, name);
    return state.attributeSet->getAttributeValue(name);
}

RuntimeValue getNumericAttributeBase(const AbilitySystemImpl& state,
                                     const std::string& name) {
    requireNumericAttribute(state, name);
    return runtimeNumber(state.baseValues.at(name));
}

void setNumericAttributeBase(AbilitySystemImpl& state, const std::string& name,
                             const RuntimeValue& value) {
    requireNumericAttribute(state, name);
    static_cast<void>(
        validateNumeric(state, name, value, "Numeric attribute base"));
    AttributeNumbers bases = state.baseValues;
    bases[name] = attributeNumber(value);
    commitBases(state, bases);
}

void setNumericAttributeBases(AbilitySystemImpl& state,
                              const RuntimeValue::Map& values) {
    AttributeNumbers bases = state.baseValues;
    for (const auto& [name, value] : values) {
        requireNumericAttribute(state, name);
        static_cast<void>(
            validateNumeric(state, name, value, "Numeric attribute base"));
        bases[name] = attributeNumber(value);
    }
    commitBases(state, bases);
}

RuntimeValue::Map getNumericAttributeBases(const AbilitySystemImpl& state) {
    return runtimeNumbers(state.baseValues);
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

void setNumericAttributeConstraint(AbilitySystemImpl& state,
                                   const std::string& name,
                                   RuntimeIdentityPtr callback) {
    requireNumericAttribute(state, name);
    if (callback == nullptr) {
        state.constraints.erase(name);
    } else {
        state.constraints[name] = RuntimeHandle(std::move(callback));
    }
    const AttributeNumbers current = preview(state, state.baseValues);
    if (applyCurrentValues(state, current, "Constraint")) {
        ++state.revision;
    }
}

int getRevision(const AbilitySystemImpl& state) {
    return state.revision;
}

void onAttributeWrite(AbilitySystemImpl& state, const std::string& name,
                      const RuntimeValue& oldValue,
                      const RuntimeValue& newValue) {
    if (state.internalAttributeWrite) {
        return;
    }
    requireNumericAttribute(state, name);
    static_cast<void>(
        validateNumeric(state, name, newValue, "Numeric attribute assignment"));
    AttributeNumbers bases = state.baseValues;
    bases[name] = attributeNumber(newValue);
    const RuntimeValue::Map overrides{{name, oldValue}};
    commitBases(state, bases, &overrides);
}

}  // namespace ludork::global::ability_system_impl
