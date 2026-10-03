#include "EnumSchema.hpp"

#include <StringUtils.hpp>

#include <cmath>
#include <stdexcept>

namespace ludork::runtime::detail {

void validateEnumModuleName(const std::string& moduleName) {
    if (!moduleName.starts_with("Enums.") || moduleName.size() == 6) {
        throw std::invalid_argument(
            "Enum must reference a module under Enums: " + moduleName);
    }
    bool first = true;
    for (const char character : moduleName) {
        if (character == '.') {
            if (first) {
                throw std::invalid_argument("Invalid enum module name: " +
                                            moduleName);
            }
            first = true;
            continue;
        }
        const bool letter = (character >= 'a' && character <= 'z') ||
                            (character >= 'A' && character <= 'Z') ||
                            character == '_';
        if (!letter && (first || character < '0' || character > '9')) {
            throw std::invalid_argument("Invalid enum module name: " +
                                        moduleName);
        }
        first = false;
    }
    if (first) {
        throw std::invalid_argument("Invalid enum module name: " + moduleName);
    }
}

TypeSchema enumValueType(RuntimeValueView values,
                         const std::string& moduleName) {
    const std::optional<RuntimeMapView> entries = values.map();
    if (!entries || entries->empty()) {
        throw std::invalid_argument("Enum must contain named scalar values: " +
                                    moduleName);
    }
    std::string category;
    bool integer = true;
    for (const auto& [key, value] : *entries) {
        if (ludork::standard::trimWhitespace(key).empty()) {
            throw std::invalid_argument("Enum keys must not be empty: " +
                                        moduleName);
        }
        std::string next;
        if (value.getIf<std::string>() != nullptr) {
            next = "string";
        } else if (value.getIf<bool>() != nullptr) {
            next = "bool";
        } else if (value.getIf<std::int64_t>() != nullptr) {
            next = "number";
        } else if (const double* number = value.getIf<double>();
                   number != nullptr && std::isfinite(*number)) {
            next = "number";
            integer = false;
        } else {
            throw std::invalid_argument("Enum value must be a finite scalar: " +
                                        moduleName + "." + key);
        }
        if (!category.empty() && category != next) {
            throw std::invalid_argument(
                "Enum values must share one scalar type: " + moduleName);
        }
        category = next;
    }
    TypeSchema result;
    result.name = category == "number" ? (integer ? "int" : "float") : category;
    return result;
}

}  // namespace ludork::runtime::detail
