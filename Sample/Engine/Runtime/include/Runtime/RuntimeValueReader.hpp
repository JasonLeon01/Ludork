#pragma once

#include <Runtime/RuntimeDataReader.hpp>
#include <Runtime/RuntimeValue.hpp>

namespace ludork::runtime::value_reader {

inline std::optional<RuntimeValueView> findValue(RuntimeMapView values,
                                                 const std::string& name) {
    return values.find(name);
}
inline RuntimeValueView requireValue(RuntimeMapView values,
                                     const std::string& name,
                                     const std::string& source) {
    const auto value = values.find(name);
    if (!value) {
        throw std::invalid_argument(source + " is missing " + name);
    }
    return *value;
}
inline RuntimeMapView requireMap(RuntimeValueView value,
                                 const std::string& source) {
    const auto map = value.map();
    if (!map) {
        throw std::invalid_argument(source + " must be an object");
    }
    return *map;
}
inline RuntimeArrayView requireArray(RuntimeValueView value,
                                     const std::string& source) {
    const auto array = value.array();
    if (!array) {
        throw std::invalid_argument(source + " must be an array");
    }
    return *array;
}

}  // namespace ludork::runtime::value_reader
