#include <Runtime/RuntimeValueReader.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ludork::runtime::value_reader {

const RuntimeValue* findValue(const RuntimeValue::Map& values,
                              const std::string& name) {
    const auto iterator = values.find(name);
    return iterator == values.end() ? nullptr : &iterator->second;
}

const RuntimeValue& requireValue(const RuntimeValue::Map& values,
                                 const std::string& name,
                                 const std::string& source) {
    const RuntimeValue* value = findValue(values, name);
    if (value == nullptr) {
        throw std::invalid_argument(source + " is missing " + name);
    }
    return *value;
}

const RuntimeValue::Map& requireMap(const RuntimeValue& value,
                                    const std::string& source) {
    const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>();
    if (map == nullptr) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}

const RuntimeValue::Array& requireArray(const RuntimeValue& value,
                                        const std::string& source) {
    const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>();
    if (array == nullptr) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source) {
    const std::string* text = value.getIf<std::string>();
    if (text == nullptr) {
        throw std::invalid_argument(source + " must be a string");
    }
    return *text;
}

bool requireBool(const RuntimeValue& value, const std::string& source) {
    const bool* boolean = value.getIf<bool>();
    if (boolean == nullptr) {
        throw std::invalid_argument(source + " must be a boolean");
    }
    return *boolean;
}

double requireNumber(const RuntimeValue& value, const std::string& source) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        if (std::isfinite(*number)) {
            return *number;
        }
    }
    throw std::invalid_argument(source + " must be a finite number");
}

float requireFloat(const RuntimeValue& value, const std::string& source) {
    const double number = requireNumber(value, source);
    if (number < -static_cast<double>(std::numeric_limits<float>::max()) ||
        number > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(source + " is outside the float range");
    }
    return static_cast<float>(number);
}

std::int64_t requireInteger(const RuntimeValue& value,
                            const std::string& source) {
    const std::int64_t* integer = value.getIf<std::int64_t>();
    if (integer == nullptr) {
        throw std::invalid_argument(source + " must be an integer");
    }
    return *integer;
}

int requireInt(const RuntimeValue& value, const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < std::numeric_limits<int>::min() ||
        integer > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(source + " is outside the int range");
    }
    return static_cast<int>(integer);
}

unsigned int requireUnsigned(const RuntimeValue& value,
                             const std::string& source) {
    const std::int64_t integer = requireInteger(value, source);
    if (integer < 0 || static_cast<std::uint64_t>(integer) >
                           std::numeric_limits<unsigned int>::max()) {
        throw std::invalid_argument(source + " must be an unsigned integer");
    }
    return static_cast<unsigned int>(integer);
}

}  // namespace ludork::runtime::value_reader
