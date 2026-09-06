#include "NumericImpl.hpp"
#include <Runtime/RuntimeReflection.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ludork::global::ability_system_impl {

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

RuntimeIdentityPtr runtimeMap(RuntimeValue::Map values) {
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

}  // namespace ludork::global::ability_system_impl
