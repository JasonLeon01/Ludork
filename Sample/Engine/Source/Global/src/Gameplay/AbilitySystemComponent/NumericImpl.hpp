#pragma once
#include <Runtime/RuntimeObject.hpp>
#include <Gameplay/GameplayModifier.hpp>
#include <Gameplay/GameplayEffectSpec.hpp>
#include <Gameplay/AttributeSet.hpp>
#include <cmath>
#include <stdexcept>

namespace ludork::global::ability_system_impl {

struct NumericValue {
    double value = 0.0;
    bool integer = true;
};
GameplayNumber resolvedNumber(const NumericValue& value);
RuntimeValue runtimeNumber(const GameplayNumber& value);
GameplayNumber attributeNumber(const RuntimeValue& value);
bool runtimeEqual(const GameplayNumber& left, const GameplayNumber& right);
bool runtimeEqual(const RuntimeValue& left, const RuntimeValue& right);
RuntimeIdentityPtr runtimeMap(RuntimeValue::Map values = {});
NumericValue unrestrictedNumeric(const GameplayNumber& value,
                                 const std::string& context);
NumericValue addNumbers(const NumericValue& left, const NumericValue& right);
NumericValue multiplyNumbers(const NumericValue& left,
                             const NumericValue& right);
NumericValue scaleMagnitude(const NumericValue& magnitude,
                            const std::string& operation, int stacks);
std::vector<RuntimeValue> invokeCallable(const RuntimeHandle& callable,
                                         std::vector<RuntimeValue> arguments);
NumericValue resolveMagnitude(const GameplayModifier& modifier,
                              const std::shared_ptr<GameplayEffectSpec>& spec,
                              int stacks);

template <typename T>
RuntimeValue runtimeObject(const std::shared_ptr<T>& value) {
    return RuntimeValue(std::static_pointer_cast<RuntimeObject>(value));
}

template <typename T>
const T* numberIf(const RuntimeValue& value) {
    return value.getIf<T>();
}

template <typename T>
const T* numberIf(const GameplayNumber& value) {
    return std::get_if<T>(&value);
}

template <typename Value>
NumericValue numericValue(const Value& value,
                          AttributeSet::NumericType schemaType,
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
    if (schemaType == AttributeSet::NumericType::Integer) {
        if (!result.integer) {
            throw std::invalid_argument(context +
                                        " must be an integer: " + name);
        }
    }
    return result;
}

}  // namespace ludork::global::ability_system_impl
