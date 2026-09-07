#include "UiControlPropertyCodec.hpp"

#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace ui_control_adapter_detail {
namespace {

const RuntimeData::Array& arrayValue(const RuntimeData& value,
                                     const std::string& source,
                                     std::size_t length) {
    const RuntimeData::Array& result =
        ludork::runtime::value_reader::requireArray(value, source);
    if (result.size() != length) {
        throw std::invalid_argument(source + " must contain " +
                                    std::to_string(length) + " values");
    }
    return result;
}

UiControlPropertyValue propertyValue(const RuntimeData& value,
                                     std::string_view type,
                                     const std::string& source) {
    if (type == "sf.IntRect" && value.isNil()) {
        return std::monostate{};
    }
    if (type == "bool") {
        return ludork::runtime::value_reader::requireBool(value, source);
    }
    if (type == "int") {
        return static_cast<std::int64_t>(
            ludork::runtime::value_reader::requireInt(value, source));
    }
    if (type == "float") {
        return static_cast<double>(
            ludork::runtime::value_reader::requireFloat(value, source));
    }
    if (type == "string" || type == "sf.Text.LineAlignment" ||
        type == "Engine.TextGradientDirection") {
        return ludork::runtime::value_reader::requireString(value, source);
    }
    if (type == "string[]") {
        const RuntimeData::Array& values =
            ludork::runtime::value_reader::requireArray(value, source);
        std::vector<std::string> result;
        result.reserve(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            result.push_back(ludork::runtime::value_reader::requireString(
                values[index], source + "[" + std::to_string(index) + "]"));
        }
        return result;
    }
    if (type == "sf.Vector2f") {
        const RuntimeData::Array& values = arrayValue(value, source, 2);
        return sf::Vector2f{ludork::runtime::value_reader::requireFloat(
                                values[0], source + "[0]"),
                            ludork::runtime::value_reader::requireFloat(
                                values[1], source + "[1]")};
    }
    if (type == "sf.Vector2u") {
        const RuntimeData::Array& values = arrayValue(value, source, 2);
        return sf::Vector2u{ludork::runtime::value_reader::requireUnsigned(
                                values[0], source + "[0]"),
                            ludork::runtime::value_reader::requireUnsigned(
                                values[1], source + "[1]")};
    }
    if (type == "sf.IntRect") {
        const RuntimeData::Array& values = arrayValue(value, source, 4);
        return sf::IntRect{{ludork::runtime::value_reader::requireInt(
                                values[0], source + "[0]"),
                            ludork::runtime::value_reader::requireInt(
                                values[1], source + "[1]")},
                           {ludork::runtime::value_reader::requireInt(
                                values[2], source + "[2]"),
                            ludork::runtime::value_reader::requireInt(
                                values[3], source + "[3]")}};
    }
    if (type == "sf.Color") {
        const RuntimeData::Array& values =
            ludork::runtime::value_reader::requireArray(value, source);
        if (values.size() != 3 && values.size() != 4) {
            throw std::invalid_argument(
                source + " must contain three or four integer channels");
        }
        auto channel = [&](std::size_t index) {
            const int number = ludork::runtime::value_reader::requireInt(
                values[index], source + "[" + std::to_string(index) + "]");
            if (number < 0 || number > 255) {
                throw std::invalid_argument(
                    source + " channels must be between 0 and 255");
            }
            return static_cast<std::uint8_t>(number);
        };
        return sf::Color{channel(0), channel(1), channel(2),
                         values.size() == 4 ? channel(3) : std::uint8_t{255}};
    }
    throw std::logic_error("Unsupported UI property schema: " +
                           std::string(type));
}

}  // namespace

UiControlProperties parseProperties(
    const UiControlAdapterDescriptor& descriptor,
    const RuntimeData::Map& properties, const std::string& source) {
    UiControlProperties result;
    result.reserve(properties.size());
    for (const auto& [name, value] : properties) {
        const auto property = std::find_if(
            descriptor.properties.begin(), descriptor.properties.end(),
            [&name](const UiControlPropertyDescriptor& candidate) {
                return candidate.id == name;
            });
        if (property == descriptor.properties.end()) {
            throw std::invalid_argument(source + " has unknown UI property " +
                                        name);
        }
        result.emplace(
            name, propertyValue(value, property->type, source + "." + name));
    }
    return result;
}

}  // namespace ui_control_adapter_detail
