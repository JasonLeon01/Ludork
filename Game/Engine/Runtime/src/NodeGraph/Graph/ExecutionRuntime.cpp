#include "ExecutionRuntime.hpp"

#include <utility>

namespace ludork::runtime::graph_detail {

StringRestore::StringRestore(std::string& value)
    : value_(value), previous_(value) {}

StringRestore::~StringRestore() {
    value_ = std::move(previous_);
}

bool runtimeValueEqual(const RuntimeValue& left, const RuntimeValue& right) {
    if (left.isNil() || right.isNil()) {
        return left.isNil() && right.isNil();
    }
    if (const bool* leftValue = left.getIf<bool>()) {
        const bool* rightValue = right.getIf<bool>();
        return rightValue != nullptr && *leftValue == *rightValue;
    }
    if (const std::int64_t* leftValue = left.getIf<std::int64_t>()) {
        if (const std::int64_t* rightValue = right.getIf<std::int64_t>()) {
            return *leftValue == *rightValue;
        }
        if (const double* rightValue = right.getIf<double>()) {
            return static_cast<double>(*leftValue) == *rightValue;
        }
        return false;
    }
    if (const double* leftValue = left.getIf<double>()) {
        if (const double* rightValue = right.getIf<double>()) {
            return *leftValue == *rightValue;
        }
        if (const std::int64_t* rightValue = right.getIf<std::int64_t>()) {
            return *leftValue == static_cast<double>(*rightValue);
        }
        return false;
    }
    if (const std::string* leftValue = left.getIf<std::string>()) {
        const std::string* rightValue = right.getIf<std::string>();
        return rightValue != nullptr && *leftValue == *rightValue;
    }
    if (const RuntimeHandle* leftValue = left.getIf<RuntimeHandle>()) {
        const RuntimeHandle* rightValue = right.getIf<RuntimeHandle>();
        if (rightValue == nullptr || leftValue->identity() == nullptr ||
            rightValue->identity() == nullptr) {
            return rightValue != nullptr &&
                   leftValue->identity() == rightValue->identity();
        }
        return leftValue->identity()->equals(*rightValue->identity());
    }
    if (const RuntimeValue::Object* leftValue =
            left.getIf<RuntimeValue::Object>()) {
        const RuntimeValue::Object* rightValue =
            right.getIf<RuntimeValue::Object>();
        return rightValue != nullptr && *leftValue == *rightValue;
    }
    return false;
}

RuntimeValue normaliseMatchValue(const RuntimeValue& value) {
    const std::string* text = value.getIf<std::string>();
    if (text != nullptr && *text == "nil") {
        return RuntimeValue();
    }
    return value;
}

}  // namespace ludork::runtime::graph_detail
