#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace ludork::runtime::value_reader {

template <typename Value>
const Value* findValue(const std::unordered_map<std::string, Value>& values,
                       const std::string& name) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? nullptr : &iterator->second;
}

template <typename Value>
const Value& requireValue(const std::unordered_map<std::string, Value>& values,
                          const std::string& name, const std::string& source) {
    const Value* value = findValue(values, name);
    if (value == nullptr) {
        throw std::invalid_argument(source + " is missing " + name);
    }
    return *value;
}

template <typename Value>
const typename Value::Map& requireMap(const Value& value,
                                      const std::string& source) {
    const typename Value::Map* map =
        value.template getIf<typename Value::Map>();
    if (map == nullptr) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}

template <typename Value>
const typename Value::Array& requireArray(const Value& value,
                                          const std::string& source) {
    const typename Value::Array* array =
        value.template getIf<typename Value::Array>();
    if (array == nullptr) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

template <typename Value>
const std::string& requireString(const Value& value,
                                 const std::string& source) {
    const std::string* text = value.template getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

template <typename Value>
bool requireBool(const Value& value, const std::string& source) {
    const bool* boolean = value.template getIf<bool>();
    if (boolean == nullptr) {
        throw std::invalid_argument(source + " must be a boolean");
    }
    return *boolean;
}

template <typename Value>
double requireNumber(const Value& value, const std::string& source) {
    if (const std::int64_t* integer = value.template getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    if (const double* number = value.template getIf<double>()) {
        if (std::isfinite(*number)) {
            return *number;
        }
    }
    throw std::invalid_argument(source + " must be a finite number");
}

template <typename Value>
float requireFloat(const Value& value, const std::string& source) {
    const double number = requireNumber(value, source);
    if (number < -static_cast<double>(std::numeric_limits<float>::max()) ||
        number > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(source + " is outside the float range");
    }
    return static_cast<float>(number);
}

template <typename Value>
std::int64_t requireInteger(const Value& value, const std::string& source) {
    const std::int64_t* integer = value.template getIf<std::int64_t>();
    if (integer == nullptr) {
        throw std::invalid_argument(source + " must be an integer");
    }
    return *integer;
}

template <typename Value>
int requireInt(const Value& value, const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < std::numeric_limits<int>::min() ||
        integer > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " is outside the int range");
    }
    return static_cast<int>(integer);
}

template <typename Value>
unsigned int requireUnsigned(const Value& value, const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < 0 || static_cast<std::uint64_t>(integer) >
                           std::numeric_limits<unsigned int>::max()) {
        throw std::invalid_argument(source + " must be an unsigned integer");
    }
    return static_cast<unsigned int>(integer);
}

}  // namespace ludork::runtime::value_reader
