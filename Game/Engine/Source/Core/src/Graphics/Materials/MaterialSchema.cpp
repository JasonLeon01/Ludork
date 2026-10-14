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
    return {port("BaseColor", "vec3"),   port("Alpha", "float"),
            port("Emission", "vec3"),    port("LightingWeight", "float"),
            port("LightBlock", "float"), port("ReflectionStrength", "float"),
            port("VertexOffset", "vec2")};
}

RuntimeData composeSize() {
    RuntimeData result = port("size", "integer", RuntimeData(std::int64_t{3}));
    RuntimeData::Array choices;
    for (int width : composeWidths) {
        choices.emplace_back(static_cast<std::int64_t>(width));
    }
    result.getMutableIf<RuntimeData::Map>()->emplace(
        "options", RuntimeData(std::move(choices)));
    return result;
}

RuntimeData swizzleChannels() {
    RuntimeData result = port("channels", "string", RuntimeData("rgb"));
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
        node("Surface", "Output", surfacePorts(), {}),
        node("SurfaceInput", "Input", {}, surfacePorts()),
        node("Constant", "Input", {}, {port("Value", "numeric")},
             {port("value", "numeric", RuntimeData(0.0))}),
        node("Parameter", "Input", {}, {port("Value", "parameter")},
             {port("id", "string", RuntimeData(""))})};
    for (const char* type :
         {"Time", "LocalUV", "LocalPosition", "WorldPosition", "FrameSize"}) {
        result.push_back(node(
            type, "Input", {},
            {port("Value", std::string(type) == "Time" ? "float" : "vec2")}));
    }
    RuntimeData::Array sampleOutputs{
        port("Color", "vec3"), port("Alpha", "float"), port("RGBA", "vec4")};
    result.push_back(
        node("SampleSprite", "Texture", {port("UV", "vec2")}, sampleOutputs));
    result.push_back(node("SampleTexture", "Texture",
                          {port("Texture", "texture"), port("UV", "vec2")},
                          sampleOutputs));
    for (const char* type :
         {"Add", "Subtract", "Multiply", "Divide", "Min", "Max", "Dot"}) {
        result.push_back(node(
            type, "Math",
            {port("A", "numeric", RuntimeData(0.0)),
             port("B", "numeric",
                  RuntimeData(std::string(type) == "Divide" ? 1.0 : 0.0))},
            {port("Value", std::string(type) == "Dot" ? "float" : "numeric")}));
    }
    for (const char* type : {"Abs", "Floor", "Fract", "Sin"}) {
        result.push_back(node(type, "Math",
                              {port("Value", "numeric", RuntimeData(0.0))},
                              {port("Value", "numeric")}));
    }
    result.push_back(node("Clamp", "Math",
                          {port("Value", "numeric", RuntimeData(0.0)),
                           port("Min", "numeric", RuntimeData(0.0)),
                           port("Max", "numeric", RuntimeData(1.0))},
                          {port("Value", "numeric")}));
    result.push_back(node("Smoothstep", "Math",
                          {port("Min", "numeric", RuntimeData(0.0)),
                           port("Max", "numeric", RuntimeData(1.0)),
                           port("Value", "numeric", RuntimeData(0.0))},
                          {port("Value", "numeric")}));
    result.push_back(node("Lerp", "Math",
                          {port("A", "numeric", RuntimeData(0.0)),
                           port("B", "numeric", RuntimeData(1.0)),
                           port("T", "numeric", RuntimeData(0.5))},
                          {port("Value", "numeric")}));
    result.push_back(node("Compose", "Vector",
                          {port("X", "float", RuntimeData(0.0)),
                           port("Y", "float", RuntimeData(0.0)),
                           port("Z", "float", RuntimeData(0.0)),
                           port("W", "float", RuntimeData(1.0))},
                          {port("Value", "numeric")}, {composeSize()}));
    result.push_back(node("Swizzle", "Vector",
                          {port("Value", "numeric", vector({0, 0, 0, 1}))},
                          {port("Value", "numeric")}, {swizzleChannels()}));
    result.push_back(node("HueShift", "Color",
                          {port("Color", "vec3", vector({1, 1, 1})),
                           port("Hue", "float", RuntimeData(0.0))},
                          {port("Value", "vec3")}));
    for (RuntimeData& definition : result) {
        RuntimeData::Map& fields = *definition.getMutableIf<RuntimeData::Map>();
        const std::string& type = *fields.at("type").getIf<std::string>();
        const bool fragmentOnly = type == "LocalUV" || type == "FrameSize" ||
                                  type == "SampleSprite" ||
                                  type == "SampleTexture";
        fields.emplace(
            "stages",
            RuntimeData(fragmentOnly
                            ? RuntimeData::Array{RuntimeData("fragment")}
                            : RuntimeData::Array{RuntimeData("vertex"),
                                                 RuntimeData("fragment")}));
        if (type == "Surface") {
            for (RuntimeData& entry :
                 *fields.at("inputs").getMutableIf<RuntimeData::Array>()) {
                RuntimeData::Map& input =
                    *entry.getMutableIf<RuntimeData::Map>();
                const std::string& id = *input.at("id").getIf<std::string>();
                input.emplace("inherited", RuntimeData(id != "VertexOffset"));
                if (id == "BaseColor") {
                    input.emplace("default", vector({1, 1, 1}));
                } else if (id == "Emission") {
                    input.emplace("default", vector({0, 0, 0}));
                } else if (id == "VertexOffset") {
                    input.emplace("default", vector({0, 0}));
                } else {
                    input.emplace(
                        "default",
                        RuntimeData(id == "Alpha" || id == "LightingWeight"
                                        ? 1.0
                                        : 0.0));
                }
                input.emplace(
                    "stages",
                    RuntimeData(RuntimeData::Array{RuntimeData(
                        id == "VertexOffset" ? "vertex" : "fragment")}));
            }
        }
        if (type == "SurfaceInput") {
            for (RuntimeData& entry :
                 *fields.at("outputs").getMutableIf<RuntimeData::Array>()) {
                RuntimeData::Map& output =
                    *entry.getMutableIf<RuntimeData::Map>();
                const std::string& id = *output.at("id").getIf<std::string>();
                output.emplace(
                    "stages",
                    RuntimeData(RuntimeData::Array{RuntimeData(
                        id == "VertexOffset" ? "vertex" : "fragment")}));
            }
        }
    }
    return result;
}

RuntimeData::Array renderingFields() {
    RuntimeData offset = port("maxVertexOffset", "vec2", vector({0, 0}));
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
                  {"type", RuntimeData("Surface")},
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
    if (type == "float") {
        return 1;
    }
    if (type == "vec2") {
        return 2;
    }
    if (type == "vec3") {
        return 3;
    }
    if (type == "vec4" || type == "color") {
        return 4;
    }
    if (type == "bool") {
        return 0;
    }
    if (type == "texture") {
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
    if (type == "texture") {
        const std::string& path = *value.getIf<std::string>();
        if (!path.empty()) {
            static_cast<void>(ludork::runtime::AssetPath::parse(path));
        }
    }
    if (type == "color") {
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
