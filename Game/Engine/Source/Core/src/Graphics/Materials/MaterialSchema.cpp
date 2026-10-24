#include <Graphics/Materials/MaterialSurfaceField.hpp>
#include "MaterialSchema.hpp"

#include <Runtime/AssetPath.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <stdexcept>

namespace ludork::engine::materials::schema {
namespace {

RuntimeData port(const char* id, const char* type, RuntimeData value = {}) {
    RuntimeData::Map result{{"id", RuntimeData(id)},
                            {"type", RuntimeData(type)}};
    if (!value.isNil()) {
        result.emplace("default", std::move(value));
    }
    return RuntimeData(std::move(result));
}

RuntimeData node(const char* type, const char* category,
                 RuntimeData::Array inputs, RuntimeData::Array outputs,
                 RuntimeData::Array properties = {}) {
    return RuntimeData(
        RuntimeData::Map{{"type", RuntimeData(type)},
                         {"displayName", RuntimeData(type)},
                         {"category", RuntimeData(category)},
                         {"inputs", RuntimeData(std::move(inputs))},
                         {"outputs", RuntimeData(std::move(outputs))},
                         {"properties", RuntimeData(std::move(properties))}});
}

RuntimeData vector(std::initializer_list<double> values) {
    RuntimeData::Array result;
    for (double value : values) {
        result.emplace_back(value);
    }
    return RuntimeData(std::move(result));
}

RuntimeData::Array surfacePorts() {
    return {
        port(surface_field_name::BaseColor, schema::value_type::Vec3),
        port(surface_field_name::Alpha, schema::value_type::Float),
        port(surface_field_name::Emission, schema::value_type::Vec3),
        port(surface_field_name::LightingWeight, schema::value_type::Float),
        port(surface_field_name::LightBlock, schema::value_type::Float),
        port(surface_field_name::ReflectionStrength, schema::value_type::Float),
        port(surface_field_name::VertexOffset, schema::value_type::Vec2)};
}

RuntimeData composeSize() {
    RuntimeData result =
        port("size", schema::value_type::Integer, RuntimeData(std::int64_t{3}));
    RuntimeData::Array choices;
    for (int width : composeWidths) {
        choices.emplace_back(static_cast<std::int64_t>(width));
    }
    result.getMutableIf<RuntimeData::Map>()->emplace(
        "options", RuntimeData(std::move(choices)));
    return result;
}

RuntimeData swizzleChannels() {
    RuntimeData result =
        port("channels", schema::value_type::String, RuntimeData("rgb"));
    RuntimeData::Array lengths;
    RuntimeData::Array channels;
    for (std::size_t index = 0; index < positionChannels.size(); ++index) {
        lengths.emplace_back(static_cast<std::int64_t>(index + 1));
        const std::string name(1, positionChannels[index]);
        const std::string alias(1, colorChannels[index]);
        const std::string label =
            std::string(
                1, static_cast<char>(positionChannels[index] - 'a' + 'A')) +
            "/" +
            std::string(1, static_cast<char>(colorChannels[index] - 'a' + 'A'));
        channels.emplace_back(RuntimeData::Map{
            {"label", RuntimeData(label)},
            {"value", RuntimeData(name)},
            {"aliases", RuntimeData(RuntimeData::Array{RuntimeData(alias)})}});
    }
    result.getMutableIf<RuntimeData::Map>()->emplace(
        "editor", RuntimeData(RuntimeData::Map{
                      {"kind", RuntimeData("swizzle")},
                      {"lengths", RuntimeData(std::move(lengths))},
                      {"channels", RuntimeData(std::move(channels))}}));
    return result;
}

}  // namespace

RuntimeData::Array nodeDefinitions() {
    RuntimeData::Array result{
        node(schema::node_type::Surface, schema::category::Output,
             surfacePorts(), {}),
        node(schema::node_type::SurfaceInput, schema::category::Input, {},
             surfacePorts()),
        node(schema::node_type::Constant, schema::category::Input, {},
             {port(schema::port_name::Value, schema::value_type::Numeric)},
             {port("value", schema::value_type::Numeric, RuntimeData(0.0))}),
        node(schema::node_type::Parameter, schema::category::Input, {},
             {port(schema::port_name::Value, schema::value_type::Parameter)},
             {port("id", schema::value_type::String, RuntimeData(""))})};
    for (const char* type :
         {schema::node_type::Time, schema::node_type::LocalUV,
          schema::node_type::LocalPosition, schema::node_type::WorldPosition,
          schema::node_type::FrameSize}) {
        result.push_back(node(type, schema::category::Input, {},
                              {port(schema::port_name::Value,
                                    std::string(type) == schema::node_type::Time
                                        ? schema::value_type::Float
                                        : schema::value_type::Vec2)}));
    }
    RuntimeData::Array sampleOutputs{
        port(schema::port_name::Color, schema::value_type::Vec3),
        port(schema::port_name::Alpha, schema::value_type::Float),
        port(schema::port_name::RGBA, schema::value_type::Vec4)};
    result.push_back(
        node(schema::node_type::SampleSprite, schema::category::Texture,
             {port(schema::port_name::UV, schema::value_type::Vec2)},
             sampleOutputs));
    result.push_back(
        node(schema::node_type::SampleTexture, schema::category::Texture,
             {port(schema::port_name::Texture, schema::value_type::Texture),
              port(schema::port_name::UV, schema::value_type::Vec2)},
             sampleOutputs));
    for (const char* type :
         {schema::node_type::Add, schema::node_type::Subtract,
          schema::node_type::Multiply, schema::node_type::Divide,
          schema::node_type::Min, schema::node_type::Max,
          schema::node_type::Dot}) {
        result.push_back(node(
            type, schema::category::Math,
            {port(schema::port_name::A, schema::value_type::Numeric,
                  RuntimeData(0.0)),
             port(schema::port_name::B, schema::value_type::Numeric,
                  RuntimeData(std::string(type) == schema::node_type::Divide
                                  ? 1.0
                                  : 0.0))},
            {port(schema::port_name::Value,
                  std::string(type) == schema::node_type::Dot
                      ? schema::value_type::Float
                      : schema::value_type::Numeric)}));
    }
    for (const char* type :
         {schema::node_type::Abs, schema::node_type::Floor,
          schema::node_type::Fract, schema::node_type::Sin}) {
        result.push_back(node(
            type, schema::category::Math,
            {port(schema::port_name::Value, schema::value_type::Numeric,
                  RuntimeData(0.0))},
            {port(schema::port_name::Value, schema::value_type::Numeric)}));
    }
    result.push_back(
        node(schema::node_type::Clamp, schema::category::Math,
             {port(schema::port_name::Value, schema::value_type::Numeric,
                   RuntimeData(0.0)),
              port(schema::port_name::Min, schema::value_type::Numeric,
                   RuntimeData(0.0)),
              port(schema::port_name::Max, schema::value_type::Numeric,
                   RuntimeData(1.0))},
             {port(schema::port_name::Value, schema::value_type::Numeric)}));
    result.push_back(
        node(schema::node_type::Smoothstep, schema::category::Math,
             {port(schema::port_name::Min, schema::value_type::Numeric,
                   RuntimeData(0.0)),
              port(schema::port_name::Max, schema::value_type::Numeric,
                   RuntimeData(1.0)),
              port(schema::port_name::Value, schema::value_type::Numeric,
                   RuntimeData(0.0))},
             {port(schema::port_name::Value, schema::value_type::Numeric)}));
    result.push_back(
        node(schema::node_type::Lerp, schema::category::Math,
             {port(schema::port_name::A, schema::value_type::Numeric,
                   RuntimeData(0.0)),
              port(schema::port_name::B, schema::value_type::Numeric,
                   RuntimeData(1.0)),
              port("T", schema::value_type::Numeric, RuntimeData(0.5))},
             {port(schema::port_name::Value, schema::value_type::Numeric)}));
    result.push_back(
        node(schema::node_type::Compose, "Vector",
             {port("X", schema::value_type::Float, RuntimeData(0.0)),
              port("Y", schema::value_type::Float, RuntimeData(0.0)),
              port("Z", schema::value_type::Float, RuntimeData(0.0)),
              port("W", schema::value_type::Float, RuntimeData(1.0))},
             {port(schema::port_name::Value, schema::value_type::Numeric)},
             {composeSize()}));
    result.push_back(
        node(schema::node_type::Swizzle, "Vector",
             {port(schema::port_name::Value, schema::value_type::Numeric,
                   vector({0, 0, 0, 1}))},
             {port(schema::port_name::Value, schema::value_type::Numeric)},
             {swizzleChannels()}));
    result.push_back(
        node(schema::node_type::HueShift, schema::port_name::Color,
             {port(schema::port_name::Color, schema::value_type::Vec3,
                   vector({1, 1, 1})),
              port("Hue", schema::value_type::Float, RuntimeData(0.0))},
             {port(schema::port_name::Value, schema::value_type::Vec3)}));
    for (RuntimeData& definition : result) {
        RuntimeData::Map& fields = *definition.getMutableIf<RuntimeData::Map>();
        const std::string& type = *fields.at("type").getIf<std::string>();
        const bool fragmentOnly = type == schema::node_type::LocalUV ||
                                  type == schema::node_type::FrameSize ||
                                  type == schema::node_type::SampleSprite ||
                                  type == schema::node_type::SampleTexture;
        fields.emplace(
            "stages",
            RuntimeData(fragmentOnly
                            ? RuntimeData::Array{RuntimeData("fragment")}
                            : RuntimeData::Array{RuntimeData("vertex"),
                                                 RuntimeData("fragment")}));
        if (type == schema::node_type::Surface) {
            for (RuntimeData& entry :
                 *fields.at("inputs").getMutableIf<RuntimeData::Array>()) {
                RuntimeData::Map& input =
                    *entry.getMutableIf<RuntimeData::Map>();
                const std::string& id = *input.at("id").getIf<std::string>();
                input.emplace(
                    "inherited",
                    RuntimeData(id != surface_field_name::VertexOffset));
                if (id == surface_field_name::BaseColor) {
                    input.emplace("default", vector({1, 1, 1}));
                } else if (id == surface_field_name::Emission) {
                    input.emplace("default", vector({0, 0, 0}));
                } else if (id == surface_field_name::VertexOffset) {
                    input.emplace("default", vector({0, 0}));
                } else {
                    input.emplace(
                        "default",
                        RuntimeData(
                            id == surface_field_name::Alpha ||
                                    id == surface_field_name::LightingWeight
                                ? 1.0
                                : 0.0));
                }
                input.emplace(
                    "stages",
                    RuntimeData(RuntimeData::Array{RuntimeData(
                        id == surface_field_name::VertexOffset ? "vertex"
                                                               : "fragment")}));
            }
        }
        if (type == schema::node_type::SurfaceInput) {
            for (RuntimeData& entry :
                 *fields.at("outputs").getMutableIf<RuntimeData::Array>()) {
                RuntimeData::Map& output =
                    *entry.getMutableIf<RuntimeData::Map>();
                const std::string& id = *output.at("id").getIf<std::string>();
                output.emplace(
                    "stages",
                    RuntimeData(RuntimeData::Array{RuntimeData(
                        id == surface_field_name::VertexOffset ? "vertex"
                                                               : "fragment")}));
            }
        }
    }
    return result;
}

RuntimeData::Array renderingFields() {
    RuntimeData offset =
        port("maxVertexOffset", schema::value_type::Vec2, vector({0, 0}));
    offset.getMutableIf<RuntimeData::Map>()->emplace("min", RuntimeData(0.0));
    return {std::move(offset)};
}

RuntimeData defaultDocument() {
    return RuntimeData(RuntimeData::Map{
        {"type", RuntimeData("material")},
        {"version", RuntimeData(std::int64_t{1})},
        {"parameters", RuntimeData(RuntimeData::Array{})},
        {"graph",
         RuntimeData(RuntimeData::Map{
             {"nodes",
              RuntimeData(RuntimeData::Array{RuntimeData(RuntimeData::Map{
                  {"id", RuntimeData("surface")},
                  {"type", RuntimeData(schema::node_type::Surface)},
                  {"inputs", RuntimeData(RuntimeData::Map{})},
                  {"properties", RuntimeData(RuntimeData::Map{})}})})},
             {"output", RuntimeData("surface")}})},
        {"editor", RuntimeData(RuntimeData::Map{
                       {"positions", RuntimeData(RuntimeData::Map{
                                         {"surface", vector({400, 200})}})}})},
        {"rendering",
         RuntimeData(RuntimeData::Map{{"maxVertexOffset", vector({0, 0})}})}});
}

int typeWidth(const std::string& type) {
    if (type == schema::value_type::Float) {
        return 1;
    }
    if (type == schema::value_type::Vec2) {
        return 2;
    }
    if (type == schema::value_type::Vec3) {
        return 3;
    }
    if (type == schema::value_type::Vec4 || type == schema::value_type::Color) {
        return 4;
    }
    if (type == schema::value_type::Boolean) {
        return 0;
    }
    if (type == schema::value_type::Texture) {
        return -1;
    }
    throw std::invalid_argument("Unknown material parameter type: " + type);
}

int valueWidth(const RuntimeData& value) {
    if (value.getIf<bool>()) {
        return 0;
    }
    if (value.getIf<std::string>()) {
        return -1;
    }
    if (const RuntimeData::Array* array = value.getIf<RuntimeData::Array>()) {
        if (array->size() < 2 || array->size() > 4) {
            throw std::invalid_argument(
                "Material vector must have two to four components");
        }
        for (const RuntimeData& item : *array) {
            ludork::runtime::value_reader::requireFloat(item,
                                                        "material vector");
        }
        return static_cast<int>(array->size());
    }
    ludork::runtime::value_reader::requireFloat(value, "material scalar");
    return 1;
}

void validateParameterValue(const std::string& type, const RuntimeData& value,
                            const std::string& source) {
    if (typeWidth(type) != valueWidth(value)) {
        throw std::invalid_argument(source + " does not match parameter type " +
                                    type);
    }
    if (type == schema::value_type::Texture) {
        const std::string& path = *value.getIf<std::string>();
        if (!path.empty()) {
            static_cast<void>(ludork::runtime::AssetPath::parse(path));
        }
    }
    if (type == schema::value_type::Color) {
        for (const RuntimeData& item : *value.getIf<RuntimeData::Array>()) {
            const float channel =
                ludork::runtime::value_reader::requireFloat(item, source);
            if (channel < 0 || channel > 1) {
                throw std::invalid_argument(
                    source + " color channels must be in [0, 1]");
            }
        }
    }
}

}  // namespace ludork::engine::materials::schema
