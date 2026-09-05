#include <Gameplay/GameplayAbilitySystem.hpp>

#include <Runtime/RuntimeReflection.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace {

struct NumericValue {
    double value = 0.0;
    bool integer = true;
};

using AttributeNumber = std::variant<std::int64_t, double>;
using AttributeNumbers = std::unordered_map<std::string, AttributeNumber>;

using NumericMap = std::unordered_map<std::string, NumericValue>;

AttributeNumber resolvedNumber(const NumericValue& value) {
    if (value.integer) {
        if (value.value <
                static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
            value.value >
                static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            throw std::invalid_argument(
                "Gameplay numeric value is out of range");
        }
        return static_cast<std::int64_t>(value.value);
    }
    return value.value;
}

RuntimeValue runtimeNumber(const AttributeNumber& value) {
    return std::visit(
        [](auto number) {
            return RuntimeValue(number);
        },
        value);
}

RuntimeValue runtimeNumber(const NumericValue& value) {
    return runtimeNumber(resolvedNumber(value));
}

RuntimeValue::Map runtimeNumbers(const AttributeNumbers& values) {
    RuntimeValue::Map result;
    for (const auto& [name, value] : values) {
        result.emplace(name, runtimeNumber(value));
    }
    return result;
}

AttributeNumber attributeNumber(const RuntimeValue& value) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return *integer;
    }
    if (const double* number = value.getIf<double>()) {
        return *number;
    }
    throw std::logic_error(
        "Validated numeric attribute has an incompatible value");
}

template <typename T>
const T* numberIf(const RuntimeValue& value) {
    return value.getIf<T>();
}

template <typename T>
const T* numberIf(const AttributeNumber& value) {
    return std::get_if<T>(&value);
}

bool runtimeEqual(const AttributeNumber& left, const AttributeNumber& right) {
    return std::visit(
        [](auto a, auto b) {
            return static_cast<double>(a) == static_cast<double>(b);
        },
        left, right);
}

RuntimeValue runtimeObject(const std::shared_ptr<RuntimeObject>& value) {
    return RuntimeValue(value);
}

template <typename T>
RuntimeValue runtimeObject(const std::shared_ptr<T>& value) {
    return RuntimeValue(std::static_pointer_cast<RuntimeObject>(value));
}

RuntimeIdentityPtr runtimeMap(RuntimeValue::Map values = {}) {
    RuntimeIdentityPtr result = createRuntimeMapIdentity();
    for (auto& [name, value] : values) {
        runtimeReflection().set(
            ludork::runtime::reference::intern(RuntimeValue(result)), name,
            value);
    }
    return result;
}

bool runtimeEqual(const RuntimeValue& left, const RuntimeValue& right) {
    const std::int64_t* leftInteger = left.getIf<std::int64_t>();
    const double* leftNumber = left.getIf<double>();
    const std::int64_t* rightInteger = right.getIf<std::int64_t>();
    const double* rightNumber = right.getIf<double>();
    if (leftInteger != nullptr || leftNumber != nullptr) {
        if (rightInteger == nullptr && rightNumber == nullptr) {
            return false;
        }
        const double leftValue = leftInteger == nullptr
                                     ? *leftNumber
                                     : static_cast<double>(*leftInteger);
        const double rightValue = rightInteger == nullptr
                                      ? *rightNumber
                                      : static_cast<double>(*rightInteger);
        return leftValue == rightValue;
    }
    return runtimeReflection().equal(left, right);
}

bool tagMatches(const std::string& tag, const std::string& query) {
    return tag == query || (tag.size() > query.size() &&
                            tag.starts_with(query) && tag[query.size()] == '.');
}

template <typename Value>
NumericValue numericValue(const Value& value, const std::string& schemaType,
                          const std::string& context, const std::string& name) {
    NumericValue result;
    if (const std::int64_t* integer = numberIf<std::int64_t>(value)) {
        result.value = static_cast<double>(*integer);
        result.integer = true;
    } else if (const double* number = numberIf<double>(value)) {
        result.value = *number;
        result.integer = false;
    } else {
        throw std::invalid_argument(context + " must be a number: " + name);
    }
    if (!std::isfinite(result.value)) {
        throw std::invalid_argument(context + " must be finite: " + name);
    }
    if (schemaType == "int") {
        if (!result.integer) {
            throw std::invalid_argument(context +
                                        " must be an integer: " + name);
        }
    } else if (schemaType != "float") {
        throw std::invalid_argument("Attribute is not numeric: " + name);
    }
    return result;
}

NumericValue unrestrictedNumeric(const RuntimeValue& value,
                                 const std::string& context) {
    NumericValue result;
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        result.value = static_cast<double>(*integer);
        result.integer = true;
    } else if (const double* number = value.getIf<double>()) {
        result.value = *number;
        result.integer = false;
    } else {
        throw std::invalid_argument(context + " must be numeric");
    }
    if (!std::isfinite(result.value)) {
        throw std::invalid_argument(context + " must be finite");
    }
    return result;
}

NumericValue addNumbers(const NumericValue& left, const NumericValue& right) {
    NumericValue result{left.value + right.value,
                        left.integer && right.integer};
    if (!std::isfinite(result.value)) {
        throw std::invalid_argument("Gameplay numeric result must be finite");
    }
    return result;
}

NumericValue multiplyNumbers(const NumericValue& left,
                             const NumericValue& right) {
    NumericValue result{left.value * right.value,
                        left.integer && right.integer};
    if (!std::isfinite(result.value)) {
        throw std::invalid_argument("Gameplay numeric result must be finite");
    }
    return result;
}

NumericValue scaleMagnitude(const NumericValue& magnitude,
                            const std::string& operation, int stacks) {
    if (operation == "Add") {
        return multiplyNumbers(magnitude, {static_cast<double>(stacks), true});
    }
    if (operation == "Multiply") {
        const double value = std::pow(magnitude.value, stacks);
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "Gameplay Effect modifier magnitude must be finite");
        }
        return {value, false};
    }
    return magnitude;
}

std::vector<RuntimeValue> invokeCallable(const RuntimeHandle& callable,
                                         std::vector<RuntimeValue> arguments) {
    return runtimeReflection().invoke(callable, arguments);
}

NumericValue resolveMagnitude(const GameplayModifier& modifier,
                              const std::shared_ptr<GameplayEffectSpec>& spec,
                              int stacks) {
    RuntimeValue magnitude = modifier.magnitude;
    if (magnitude.getIf<RuntimeHandle>() != nullptr) {
        const std::vector<RuntimeValue> results =
            invokeCallable(ludork::runtime::reference::intern(magnitude),
                           {runtimeObject(spec),
                            RuntimeValue(static_cast<std::int64_t>(stacks))});
        if (results.size() != 1) {
            throw std::invalid_argument(
                "Gameplay Effect modifier magnitude function must return one "
                "value");
        }
        magnitude = results.front();
    }
    return unrestrictedNumeric(magnitude, "Gameplay Effect modifier magnitude");
}

struct ModifierAggregate {
    NumericValue additive{0.0, true};
    NumericValue multiplier{1.0, true};
    std::optional<NumericValue> overrideValue;
    std::optional<NumericValue> minimum;
};

void accumulateModifiers(ModifierAggregate& aggregate,
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

struct AbilitySystemComponent::State {
    struct Listener {
        RuntimeHandle callback;
        RuntimeValue::Array params;
    };

    AbilitySystemComponent* component;
    RuntimeValue owner;
    std::shared_ptr<AttributeSet> attributeSet;
    std::vector<std::string> numericAttributes;
    AttributeNumbers baseValues;
    std::vector<std::shared_ptr<GameplayAbilitySpec>> abilities;
    std::unordered_map<int, std::shared_ptr<ActiveGameplayEffect>>
        activeEffects;
    std::vector<int> activeEffectOrder;
    std::unordered_map<std::string, int> tagCounts;
    std::unordered_map<std::string, std::vector<Listener>> listeners;
    std::unordered_map<std::string, RuntimeHandle> constraints;
    int nextAbilityOrder = 1;
    int nextEffectHandle = 1;
    int nextEffectOrder = 1;
    int revision = 0;
    bool internalAttributeWrite = false;
    bool suppressAttributeListeners = false;

    State(AbilitySystemComponent* ownerComponent, RuntimeValue systemOwner,
          std::shared_ptr<AttributeSet> attributes)
        : component(ownerComponent),
          owner(std::move(systemOwner)),
          attributeSet(std::move(attributes)) {
        if (attributeSet == nullptr) {
            throw std::invalid_argument(
                "Ability System requires an AttributeSet");
        }
        for (const std::string& name : attributeSet->getAttributeNames()) {
            const std::string type = attributeSet->getAttributeType(name);
            if (type != "int" && type != "float") {
                continue;
            }
            RuntimeValue value = attributeSet->getAttributeValue(name);
            static_cast<void>(
                numericValue(value, type, "Numeric attribute default", name));
            numericAttributes.push_back(name);
            baseValues.emplace(name, attributeNumber(value));
        }
    }

    RuntimeValue selfValue() const {
        std::shared_ptr<RuntimeObject> ownerObject = component->runtimeOwner();
        if (ownerObject == nullptr) {
            ownerObject = component->weak_from_this().lock();
        }
        if (ownerObject == nullptr) {
            throw std::logic_error(
                "Ability System has no stable runtime owner");
        }
        return runtimeObject(ownerObject);
    }

    std::shared_ptr<AbilitySystemComponent> self() const {
        std::shared_ptr<RuntimeObject> ownerObject =
            component->weak_from_this().lock();
        if (ownerObject == nullptr) {
            throw std::logic_error(
                "Ability System has no stable runtime owner");
        }
        return std::shared_ptr<AbilitySystemComponent>(std::move(ownerObject),
                                                       component);
    }

    void requireNumericAttribute(const std::string& name) const {
        if (!baseValues.contains(name)) {
            throw std::invalid_argument("Unknown numeric attribute: " + name);
        }
    }

    template <typename Value>
    NumericValue validateNumeric(const std::string& name, const Value& value,
                                 const std::string& context) const {
        return numericValue(value, attributeSet->getAttributeType(name),
                            context, name);
    }

    std::shared_ptr<ActiveGameplayEffect> activeEffect(int handle) const {
        const auto iterator = activeEffects.find(handle);
        return iterator == activeEffects.end() ? nullptr : iterator->second;
    }

    NumericValue resolveAttribute(
        const std::string& name, const AttributeNumbers& bases,
        const std::shared_ptr<GameplayEffectSpec>& pendingSpec,
        std::optional<int> replacedHandle, std::optional<int> replacementStacks,
        const AttributeNumbers& resolvedValues) const {
        ModifierAggregate aggregate;
        for (const int handle : activeEffectOrder) {
            const std::shared_ptr<ActiveGameplayEffect> active =
                activeEffect(handle);
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
            accumulateModifiers(aggregate, name, pendingSpec,
                                pendingSpec->stacks);
        }

        const auto base = bases.find(name);
        if (base == bases.end()) {
            throw std::logic_error("Numeric base is missing: " + name);
        }
        NumericValue current =
            validateNumeric(name, base->second, "Numeric attribute base");
        current = multiplyNumbers(addNumbers(current, aggregate.additive),
                                  aggregate.multiplier);
        if (aggregate.overrideValue.has_value()) {
            current = *aggregate.overrideValue;
        }
        if (aggregate.minimum.has_value() &&
            current.value < aggregate.minimum->value) {
            current = *aggregate.minimum;
        }

        const auto constraint = constraints.find(name);
        if (constraint != constraints.end()) {
            const std::vector<RuntimeValue> results =
                invokeCallable(constraint->second,
                               {runtimeNumber(current), selfValue(),
                                RuntimeValue(runtimeNumbers(resolvedValues))});
            if (results.size() != 1) {
                throw std::invalid_argument(
                    "Numeric attribute constraint must return one value");
            }
            current = validateNumeric(name, results.front(),
                                      "Numeric attribute current value");
        } else {
            current = validateNumeric(name, runtimeNumber(current),
                                      "Numeric attribute current value");
        }
        return current;
    }

    AttributeNumbers preview(
        const AttributeNumbers& bases,
        const std::shared_ptr<GameplayEffectSpec>& pendingSpec = {},
        std::optional<int> replacedHandle = std::nullopt,
        std::optional<int> replacementStacks = std::nullopt) const {
        AttributeNumbers values;
        for (const std::string& name : numericAttributes) {
            if (name != "HP") {
                values.emplace(name,
                               resolvedNumber(resolveAttribute(
                                   name, bases, pendingSpec, replacedHandle,
                                   replacementStacks, values)));
            }
        }
        if (bases.contains("HP")) {
            values.emplace("HP", resolvedNumber(resolveAttribute(
                                     "HP", bases, pendingSpec, replacedHandle,
                                     replacementStacks, values)));
        }
        return values;
    }

    void notify(const std::string& name, const RuntimeValue& oldValue,
                const RuntimeValue& newValue,
                const RuntimeValue::Map& change) const {
        const bool force = change.contains("force") &&
                           change.at("force").getIf<bool>() != nullptr &&
                           *change.at("force").getIf<bool>();
        if (suppressAttributeListeners ||
            (runtimeEqual(oldValue, newValue) && !force)) {
            return;
        }
        const auto entries = listeners.find(name);
        if (entries == listeners.end()) {
            return;
        }
        for (const Listener& listener : entries->second) {
            RuntimeValue::Array arguments{oldValue, newValue,
                                          RuntimeValue(change)};
            arguments.insert(arguments.end(), listener.params.begin(),
                             listener.params.end());
            static_cast<void>(
                invokeCallable(listener.callback, std::move(arguments)));
        }
    }

    bool applyCurrentValues(
        const AttributeNumbers& values, const std::string& source,
        const AttributeNumbers* oldBases = nullptr,
        const AttributeNumbers* newBases = nullptr,
        const RuntimeValue::Map* oldValueOverrides = nullptr) {
        RuntimeValue::Map oldValues;
        for (const std::string& name : numericAttributes) {
            const auto overrideValue = oldValueOverrides == nullptr
                                           ? RuntimeValue::Map::const_iterator{}
                                           : oldValueOverrides->find(name);
            if (oldValueOverrides != nullptr &&
                overrideValue != oldValueOverrides->end()) {
                oldValues.emplace(name, overrideValue->second);
            } else {
                oldValues.emplace(name, attributeSet->getAttributeValue(name));
            }
        }

        suppressAttributeListeners = true;
        internalAttributeWrite = true;
        try {
            for (const std::string& name : numericAttributes) {
                const auto value = values.find(name);
                if (value == values.end()) {
                    throw std::logic_error(
                        "Resolved numeric attribute is missing: " + name);
                }
                static_cast<void>(validateNumeric(
                    name, value->second, "Numeric attribute current value"));
                attributeSet->setAttributeValue(name,
                                                runtimeNumber(value->second));
            }
        } catch (...) {
            internalAttributeWrite = false;
            suppressAttributeListeners = false;
            throw;
        }
        internalAttributeWrite = false;
        suppressAttributeListeners = false;

        bool changed = false;
        for (const std::string& name : numericAttributes) {
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
            const RuntimeValue current = attributeSet->getAttributeValue(name);
            const bool fieldChanged =
                !runtimeEqual(oldValues.at(name), current) ||
                *change.at("force").getIf<bool>();
            changed = changed || fieldChanged;
            notify(name, oldValues.at(name), current, change);
        }
        return changed;
    }

    void commitBases(const AttributeNumbers& bases,
                     const RuntimeValue::Map* oldValueOverrides = nullptr) {
        const AttributeNumbers oldBases = baseValues;
        const AttributeNumbers currentValues = preview(bases);
        bool baseChanged = false;
        for (const std::string& name : numericAttributes) {
            if (!runtimeEqual(oldBases.at(name), bases.at(name))) {
                baseChanged = true;
                break;
            }
        }
        baseValues = bases;
        const bool currentChanged = applyCurrentValues(
            currentValues, "Base", &oldBases, &baseValues, oldValueOverrides);
        if (baseChanged || currentChanged) {
            ++revision;
        }
    }

    void validateAbility(
        const std::shared_ptr<GameplayAbility>& ability) const {
        if (ability == nullptr) {
            throw std::invalid_argument(
                "Ability System can only grant GameplayAbility instances");
        }
        if (ability->id.empty()) {
            throw std::invalid_argument(
                "Gameplay Ability ID must be a non-empty string");
        }
        if (!std::isfinite(ability->priority)) {
            throw std::invalid_argument(
                "Gameplay Ability priority must be finite");
        }
        for (const std::vector<std::string>* tags :
             {&ability->abilityTags, &ability->requiredTags,
              &ability->blockedTags, &ability->triggerTags}) {
            if (std::any_of(tags->begin(), tags->end(),
                            [](const std::string& tag) {
                                return tag.empty();
                            })) {
                throw std::invalid_argument(
                    "Gameplay Ability Tags must be non-empty strings");
            }
        }
    }

    void validateEffectSpec(
        const std::shared_ptr<GameplayEffectSpec>& spec) const {
        if (spec == nullptr || spec->effect == nullptr) {
            throw std::invalid_argument(
                "Ability System requires GameplayEffectSpec");
        }
        if (spec->stacks <= 0) {
            throw std::invalid_argument(
                "Gameplay Effect stacks must be positive");
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
            if (!baseValues.contains(modifier.attribute)) {
                throw std::invalid_argument(
                    "Unknown Gameplay Effect numeric attribute: " +
                    modifier.attribute);
            }
            if (modifier.operation != "Add" &&
                modifier.operation != "Multiply" &&
                modifier.operation != "Override") {
                throw std::invalid_argument(
                    "Unsupported Gameplay Effect modifier operation: " +
                    modifier.operation);
            }
            static_cast<void>(resolveMagnitude(modifier, spec, spec->stacks));
            if (!modifier.minimum.isNil()) {
                static_cast<void>(unrestrictedNumeric(
                    modifier.minimum, "Gameplay Effect modifier minimum"));
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
            validateAbility(ability);
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
        const std::shared_ptr<GameplayEffectSpec>& spec) const {
        if (spec->effect->id.empty()) {
            return nullptr;
        }
        for (const int handle : activeEffectOrder) {
            const std::shared_ptr<ActiveGameplayEffect> active =
                activeEffect(handle);
            if (active != nullptr &&
                active->spec->effect->id == spec->effect->id &&
                runtimeEqual(active->spec->sourceKey, spec->sourceKey)) {
                return active;
            }
        }
        return nullptr;
    }

    std::pair<AttributeNumbers, bool> instantBases(
        const std::shared_ptr<GameplayEffectSpec>& spec) const {
        AttributeNumbers result = baseValues;
        bool changed = false;
        for (const GameplayModifier& modifier : spec->effect->modifiers) {
            NumericValue base = validateNumeric(modifier.attribute,
                                                result.at(modifier.attribute),
                                                "Numeric attribute base");
            NumericValue magnitude =
                resolveMagnitude(modifier, spec, spec->stacks);
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
            if (!modifier.minimum.isNil()) {
                const NumericValue minimum = unrestrictedNumeric(
                    modifier.minimum, "Gameplay Effect modifier minimum");
                if (base.value < minimum.value) {
                    base = minimum;
                }
            }
            const AttributeNumber value = resolvedNumber(base);
            static_cast<void>(validateNumeric(modifier.attribute, value,
                                              "Gameplay Effect result"));
            result[modifier.attribute] = value;
            changed = true;
        }
        return {std::move(result), changed};
    }

    void changeTagCount(const std::string& tag, int delta) {
        if (tag.empty()) {
            throw std::invalid_argument("Gameplay Tag must not be empty");
        }
        const int count = tagCounts[tag] + delta;
        if (count < 0) {
            throw std::logic_error("Gameplay Tag count cannot be negative: " +
                                   tag);
        }
        if (count == 0) {
            tagCounts.erase(tag);
        } else {
            tagCounts[tag] = count;
        }
    }
};

AbilitySystemComponent::AbilitySystemComponent(
    RuntimeValue owner, std::shared_ptr<AttributeSet> attributeSet)
    : state_(std::make_unique<State>(this, std::move(owner),
                                     std::move(attributeSet))) {}

AbilitySystemComponent::~AbilitySystemComponent() = default;

RuntimeValue AbilitySystemComponent::getOwner() const {
    return state_->owner;
}

std::shared_ptr<AttributeSet> AbilitySystemComponent::getAttributeSet() const {
    return state_->attributeSet;
}

RuntimeValue AbilitySystemComponent::getNumericAttribute(
    const std::string& name) const {
    state_->requireNumericAttribute(name);
    return state_->attributeSet->getAttributeValue(name);
}

RuntimeValue AbilitySystemComponent::getNumericAttributeBase(
    const std::string& name) const {
    state_->requireNumericAttribute(name);
    return runtimeNumber(state_->baseValues.at(name));
}

void AbilitySystemComponent::setNumericAttributeBase(
    const std::string& name, const RuntimeValue& value) {
    state_->requireNumericAttribute(name);
    static_cast<void>(
        state_->validateNumeric(name, value, "Numeric attribute base"));
    AttributeNumbers bases = state_->baseValues;
    bases[name] = attributeNumber(value);
    state_->commitBases(bases);
}

void AbilitySystemComponent::setNumericAttributeBases(
    const RuntimeValue::Map& values) {
    AttributeNumbers bases = state_->baseValues;
    for (const auto& [name, value] : values) {
        state_->requireNumericAttribute(name);
        static_cast<void>(
            state_->validateNumeric(name, value, "Numeric attribute base"));
        bases[name] = attributeNumber(value);
    }
    state_->commitBases(bases);
}

RuntimeValue::Map AbilitySystemComponent::getNumericAttributeBases() const {
    return runtimeNumbers(state_->baseValues);
}

void AbilitySystemComponent::addAttributeChangeListener(
    const std::string& name, RuntimeIdentityPtr callback,
    RuntimeValue::Array params) {
    state_->requireNumericAttribute(name);
    if (callback == nullptr) {
        throw std::invalid_argument(
            "Attribute change listener must be a function");
    }
    state_->listeners[name].push_back(
        {RuntimeHandle(std::move(callback)), std::move(params)});
}

void AbilitySystemComponent::setNumericAttributeConstraint(
    const std::string& name, RuntimeIdentityPtr callback) {
    state_->requireNumericAttribute(name);
    if (callback == nullptr) {
        state_->constraints.erase(name);
    } else {
        state_->constraints[name] = RuntimeHandle(std::move(callback));
    }
    const AttributeNumbers current = state_->preview(state_->baseValues);
    if (state_->applyCurrentValues(current, "Constraint")) {
        ++state_->revision;
    }
}

std::shared_ptr<GameplayAbilitySpec> AbilitySystemComponent::giveAbility(
    std::shared_ptr<GameplayAbility> ability, RuntimeValue sourceKey) {
    state_->validateAbility(ability);
    std::shared_ptr<GameplayAbilitySpec> spec =
        std::make_shared<GameplayAbilitySpec>(std::move(ability),
                                              std::move(sourceKey),
                                              state_->nextAbilityOrder++);
    state_->abilities.push_back(spec);
    ++state_->revision;
    return spec;
}

void AbilitySystemComponent::removeAbilitiesBySource(
    const RuntimeValue& sourceKey) {
    const std::size_t oldSize = state_->abilities.size();
    std::erase_if(state_->abilities,
                  [&](const std::shared_ptr<GameplayAbilitySpec>& spec) {
                      return runtimeEqual(spec->sourceKey, sourceKey);
                  });
    if (state_->abilities.size() != oldSize) {
        ++state_->revision;
    }
}

std::shared_ptr<GameplayAbilityResult>
AbilitySystemComponent::tryActivateAbility(
    const std::string& abilityID,
    std::shared_ptr<GameplayEventData> eventData) {
    if (eventData == nullptr) {
        eventData = std::make_shared<GameplayEventData>();
    }
    std::vector<std::shared_ptr<GameplayAbilitySpec>> matches;
    for (const std::shared_ptr<GameplayAbilitySpec>& spec : state_->abilities) {
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
    if (matches.empty()) {
        return GameplayAbilityResult::Failure(
            RuntimeValue("AbilityNotFound"),
            runtimeMap({{"abilityID", RuntimeValue(abilityID)}}));
    }
    const std::shared_ptr<AbilitySystemComponent> self = state_->self();
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
        runtimeMap({{"abilityID", RuntimeValue(abilityID)}}));
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
    std::vector<std::shared_ptr<GameplayAbilitySpec>> matches;
    for (const std::shared_ptr<GameplayAbilitySpec>& spec : state_->abilities) {
        if (std::any_of(spec->ability->triggerTags.begin(),
                        spec->ability->triggerTags.end(),
                        [&](const std::string& tag) {
                            return tagMatches(eventData->eventTag, tag);
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
    const std::shared_ptr<AbilitySystemComponent> self = state_->self();
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
    state_->validateEffectSpec(spec);
    if (spec->effect->durationPolicy == "Instant") {
        auto [bases, changed] = state_->instantBases(spec);
        if (changed) {
            static_cast<void>(state_->preview(bases));
        }
        return true;
    }
    const std::shared_ptr<ActiveGameplayEffect> existing =
        state_->findStackableEffect(spec);
    if (existing == nullptr) {
        static_cast<void>(state_->preview(state_->baseValues, spec));
    } else if (spec->effect->stackingPolicy == "Aggregate") {
        static_cast<void>(state_->preview(state_->baseValues, {},
                                          existing->handle,
                                          existing->stacks + spec->stacks));
    }
    return true;
}

std::optional<int> AbilitySystemComponent::applyGameplayEffectSpec(
    const std::shared_ptr<GameplayEffectSpec>& spec) {
    state_->validateEffectSpec(spec);
    if (spec->effect->durationPolicy == "Instant") {
        auto [bases, changed] = state_->instantBases(spec);
        if (changed) {
            state_->commitBases(bases);
        }
        return std::nullopt;
    }

    const std::shared_ptr<ActiveGameplayEffect> existing =
        state_->findStackableEffect(spec);
    if (existing != nullptr) {
        if (spec->effect->stackingPolicy == "Aggregate") {
            const int replacementStacks = existing->stacks + spec->stacks;
            const AttributeNumbers current = state_->preview(
                state_->baseValues, {}, existing->handle, replacementStacks);
            existing->stacks = replacementStacks;
            state_->applyCurrentValues(current, "Effect");
            ++state_->revision;
        }
        return existing->handle;
    }

    const AttributeNumbers current = state_->preview(state_->baseValues, spec);
    const int handle = state_->nextEffectHandle++;
    std::shared_ptr<ActiveGameplayEffect> active =
        std::make_shared<ActiveGameplayEffect>(handle, spec,
                                               state_->nextEffectOrder++);
    state_->activeEffects.emplace(handle, active);
    state_->activeEffectOrder.push_back(handle);
    for (const std::string& tag : spec->effect->grantedTags) {
        state_->changeTagCount(tag, 1);
    }
    for (const std::shared_ptr<GameplayAbility>& ability :
         spec->effect->grantedAbilities) {
        active->grantedAbilitySpecs.push_back(
            giveAbility(ability, runtimeObject(active)));
    }
    state_->applyCurrentValues(current, "Effect");
    ++state_->revision;
    return handle;
}

bool AbilitySystemComponent::removeActiveGameplayEffect(
    int handle, std::optional<int> stacks) {
    const std::shared_ptr<ActiveGameplayEffect> active =
        state_->activeEffect(handle);
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
        const AttributeNumbers current = state_->preview(
            state_->baseValues, {}, active->handle, replacementStacks);
        active->stacks = replacementStacks;
        state_->applyCurrentValues(current, "Effect");
        ++state_->revision;
        return true;
    }

    const AttributeNumbers current =
        state_->preview(state_->baseValues, {}, active->handle, 0);
    state_->activeEffects.erase(handle);
    std::erase(state_->activeEffectOrder, handle);
    for (const std::string& tag : active->spec->effect->grantedTags) {
        state_->changeTagCount(tag, -1);
    }
    removeAbilitiesBySource(runtimeObject(active));
    state_->applyCurrentValues(current, "Effect");
    ++state_->revision;
    return true;
}

int AbilitySystemComponent::getActiveEffectStacks(
    const std::string& effectID) const {
    int result = 0;
    for (const int handle : state_->activeEffectOrder) {
        const std::shared_ptr<ActiveGameplayEffect> active =
            state_->activeEffect(handle);
        if (active != nullptr && active->spec->effect->id == effectID) {
            result += active->stacks;
        }
    }
    return result;
}

std::vector<std::shared_ptr<ActiveGameplayEffect>>
AbilitySystemComponent::getActiveGameplayEffects() const {
    std::vector<std::shared_ptr<ActiveGameplayEffect>> result;
    result.reserve(state_->activeEffectOrder.size());
    for (const int handle : state_->activeEffectOrder) {
        const std::shared_ptr<ActiveGameplayEffect> active =
            state_->activeEffect(handle);
        if (active != nullptr) {
            result.push_back(active);
        }
    }
    return result;
}

bool AbilitySystemComponent::hasMatchingGameplayTag(
    const std::string& tag) const {
    return std::any_of(state_->tagCounts.begin(), state_->tagCounts.end(),
                       [&](const auto& entry) {
                           return entry.second > 0 &&
                                  tagMatches(entry.first, tag);
                       });
}

int AbilitySystemComponent::getRevision() const {
    return state_->revision;
}

void AbilitySystemComponent::onAttributeWrite(const std::string& name,
                                              const RuntimeValue& oldValue,
                                              const RuntimeValue& newValue) {
    if (state_->internalAttributeWrite) {
        return;
    }
    state_->requireNumericAttribute(name);
    static_cast<void>(state_->validateNumeric(name, newValue,
                                              "Numeric attribute assignment"));
    AttributeNumbers bases = state_->baseValues;
    bases[name] = attributeNumber(newValue);
    const RuntimeValue::Map overrides{{name, oldValue}};
    state_->commitBases(bases, &overrides);
}
