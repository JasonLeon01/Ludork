#include <Graphics/Materials/MaterialResource.hpp>
#include <Graphics/Materials/MaterialCompiler.hpp>
#include "MaterialSchema.hpp"

#include <Runtime/ConcurrentResourceCache.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace ludork::engine::materials {
namespace {

namespace reader = ludork::runtime::value_reader;

thread_local std::vector<const MaterialResource::ScopedOverrides*> overrides;

ludork::runtime::ConcurrentResourceCache<const MaterialResource, true>&
resourceCache() {
    static ludork::runtime::ConcurrentResourceCache<const MaterialResource,
                                                    true>
        cache;
    return cache;
}

const RuntimeData& required(const RuntimeData::Map& map,
                            const std::string& key) {
    return reader::requireValue(map, key, "material");
}

std::string text(const RuntimeData::Map& map, const std::string& key) {
    return reader::requireString(required(map, key), "material." + key);
}

RuntimeData::Map optionalMap(const RuntimeData::Map& map,
                             const std::string& key) {
    const RuntimeData* value = reader::findValue(map, key);
    return value ? reader::requireMap(*value, "material." + key)
                 : RuntimeData::Map{};
}

void identifier(const std::string& value, const std::string& source) {
    if (value.empty()) {
        throw std::invalid_argument(source + " must not be empty");
    }
}

void checkRange(const MaterialResource::Parameter& parameter,
                const RuntimeData& value) {
    schema::validateParameterValue(parameter.type, value,
                                   "material parameter " + parameter.id);
    const auto check = [&parameter](const RuntimeData& component) {
        const float number = reader::requireFloat(component, parameter.id);
        if ((parameter.minimum && number < *parameter.minimum) ||
            (parameter.maximum && number > *parameter.maximum)) {
            throw std::invalid_argument(
                "Material parameter outside declared range: " + parameter.id);
        }
    };
    if (schema::typeWidth(parameter.type) > 0) {
        if (const auto* array = value.getIf<RuntimeData::Array>()) {
            for (const RuntimeData& component : *array) {
                check(component);
            }
        } else {
            check(value);
        }
    }
}

}  // namespace

std::shared_ptr<const MaterialResource> MaterialResource::load(
    const std::string& path) {
    if (path.empty()) {
        return defaultMaterial();
    }
    for (auto scope = overrides.rbegin(); scope != overrides.rend(); ++scope) {
        if (const auto found = (*scope)->resources_.find(path);
            found != (*scope)->resources_.end()) {
            return found->second;
        }
        if (const auto document = (*scope)->documents_.find(path);
            document != (*scope)->documents_.end()) {
            const auto resource = fromData(document->second, path);
            (*scope)->resources_.emplace(path, resource);
            return resource;
        }
    }
    return resourceCache().getOrLoad(path, [&path]() {
        return fromData(loadData(path), path);
    });
}

MaterialResource::ScopedOverrides::ScopedOverrides(
    const RuntimeData::Map& documents)
    : documents_(documents) {
    for (const auto& [path, data] : documents_) {
        static_cast<void>(data);
        if (path.empty()) {
            throw std::invalid_argument(
                "Material preview override requires a resource path");
        }
    }
    overrides.push_back(this);
}

MaterialResource::ScopedOverrides::~ScopedOverrides() {
    std::erase(overrides, this);
}

std::shared_ptr<const MaterialResource> MaterialResource::fromData(
    const RuntimeData& data, std::string path) {
    const RuntimeData::Map& root = reader::requireMap(data, "material");
    if (text(root, "type") != "material" ||
        reader::requireInt(required(root, "version"), "material.version") !=
            1) {
        throw std::invalid_argument(
            "Material requires type material and version 1");
    }
    std::shared_ptr<MaterialResource> result(new MaterialResource());
    result->data_ = data;
    result->path_ = std::move(path);
    std::unordered_set<std::string> parameterIds;
    RuntimeData::Array parameterSignatures;
    for (const RuntimeData& entry : reader::requireArray(
             required(root, "parameters"), "material.parameters")) {
        const RuntimeData::Map& fields =
            reader::requireMap(entry, "material parameter");
        Parameter parameter;
        parameter.id = text(fields, "id");
        identifier(parameter.id, "material parameter id");
        if (!parameterIds.insert(parameter.id).second) {
            throw std::invalid_argument("Duplicate material parameter: " +
                                        parameter.id);
        }
        parameter.type = text(fields, "type");
        const RuntimeData* displayName =
            reader::findValue(fields, "displayName");
        parameter.displayName =
            displayName
                ? reader::requireString(*displayName, "parameter.displayName")
                : parameter.id;
        parameter.defaultValue = required(fields, "default");
        if (const RuntimeData* group = reader::findValue(fields, "group")) {
            parameter.group = reader::requireString(*group, "parameter.group");
        }
        if (const RuntimeData* minimum = reader::findValue(fields, "min")) {
            parameter.minimum = reader::requireFloat(*minimum, "parameter.min");
        }
        if (const RuntimeData* maximum = reader::findValue(fields, "max")) {
            parameter.maximum = reader::requireFloat(*maximum, "parameter.max");
        }
        if (parameter.minimum && parameter.maximum &&
            *parameter.minimum > *parameter.maximum) {
            throw std::invalid_argument("Material parameter min exceeds max: " +
                                        parameter.id);
        }
        if ((parameter.minimum || parameter.maximum) &&
            schema::typeWidth(parameter.type) <= 0) {
            throw std::invalid_argument(
                "Material parameter range requires a numeric type: " +
                parameter.id);
        }
        checkRange(parameter, parameter.defaultValue);
        RuntimeData::Map signature = fields;
        signature.erase("displayName");
        signature.erase("group");
        parameterSignatures.emplace_back(std::move(signature));
        result->parameters_.push_back(std::move(parameter));
    }
    const RuntimeData::Map& graph =
        reader::requireMap(required(root, "graph"), "material.graph");
    result->output_ = text(graph, "output");
    std::unordered_set<std::string> nodeIds;
    std::unordered_set<std::string> nodeTypes;
    for (const RuntimeData& definition : schema::nodeDefinitions()) {
        nodeTypes.insert(text(*definition.getIf<RuntimeData::Map>(), "type"));
    }
    for (const RuntimeData& entry : reader::requireArray(
             required(graph, "nodes"), "material.graph.nodes")) {
        const RuntimeData::Map& fields =
            reader::requireMap(entry, "material node");
        Node node;
        node.id = text(fields, "id");
        node.type = text(fields, "type");
        identifier(node.id, "material node id");
        if (!nodeIds.insert(node.id).second) {
            throw std::invalid_argument("Duplicate material node: " + node.id);
        }
        if (!nodeTypes.contains(node.type)) {
            throw std::invalid_argument("Unknown material node type: " +
                                        node.type);
        }
        node.inputs = optionalMap(fields, "inputs");
        node.properties = optionalMap(fields, "properties");
        result->nodes_.push_back(std::move(node));
    }
    const RuntimeData::Map rendering = optionalMap(root, "rendering");
    if (const RuntimeData* offset =
            reader::findValue(rendering, "maxVertexOffset")) {
        schema::validateParameterValue("vec2", *offset,
                                       "material.rendering.maxVertexOffset");
        const auto& values = *offset->getIf<RuntimeData::Array>();
        result->maximumVertexOffset_ = {
            reader::requireFloat(values[0], "maxVertexOffset.x"),
            reader::requireFloat(values[1], "maxVertexOffset.y")};
        if (result->maximumVertexOffset_.x < 0 ||
            result->maximumVertexOffset_.y < 0) {
            throw std::invalid_argument(
                "Material maximum vertex offset must be non-negative");
        }
    }
    result->signature_ = stringifyJSON(RuntimeData(RuntimeData::Map{
        {"graph", required(root, "graph")},
        {"parameters", RuntimeData(std::move(parameterSignatures))},
        {"rendering", RuntimeData(rendering)}}));
    static_cast<void>(MaterialCompiler::compile(*result));
    return result;
}

std::shared_ptr<const MaterialResource> MaterialResource::defaultMaterial() {
    static const std::shared_ptr<const MaterialResource> value =
        fromData(schema::defaultDocument());
    return value;
}

RuntimeData MaterialResource::nodeRegistry() {
    RuntimeData::Array types;
    RuntimeData::Map defaults;
    RuntimeData::Array fields;
    for (const auto& [id, type, required] :
         {std::tuple{"id", "string", true},
          std::tuple{"displayName", "string", false},
          std::tuple{"type", "parameterType", true},
          std::tuple{"default", "parameterValue", true},
          std::tuple{"group", "string", false},
          std::tuple{"min", "float", false},
          std::tuple{"max", "float", false}}) {
        fields.emplace_back(
            RuntimeData::Map{{"id", RuntimeData(id)},
                             {"type", RuntimeData(type)},
                             {"required", RuntimeData(required)}});
    }
    for (const char* type :
         {"float", "vec2", "vec3", "vec4", "color", "bool", "texture"}) {
        types.emplace_back(type);
        const int width = schema::typeWidth(type);
        if (width == 0) {
            defaults.emplace(type, RuntimeData(false));
        } else if (width < 0) {
            defaults.emplace(type, RuntimeData(""));
        } else if (width == 1) {
            defaults.emplace(type, RuntimeData(0.0));
        } else {
            RuntimeData::Array values(
                static_cast<std::size_t>(width),
                RuntimeData(std::string(type) == "color" ? 1.0 : 0.0));
            defaults.emplace(type, RuntimeData(std::move(values)));
        }
    }
    return RuntimeData(RuntimeData::Map{
        {"formatVersion", RuntimeData(std::int64_t{1})},
        {"nodes", RuntimeData(schema::nodeDefinitions())},
        {"renderingFields", RuntimeData(schema::renderingFields())},
        {"defaultMaterial", schema::defaultDocument()},
        {"parameterTypes", RuntimeData(std::move(types))},
        {"parameterFields", RuntimeData(std::move(fields))},
        {"rangeParameterTypes",
         RuntimeData(RuntimeData::Array{
             RuntimeData("float"), RuntimeData("vec2"), RuntimeData("vec3"),
             RuntimeData("vec4"), RuntimeData("color")})},
        {"parameterDefaults", RuntimeData(std::move(defaults))}});
}

void MaterialResource::clearCache() {
    resourceCache().clear();
}
const std::string& MaterialResource::path() const {
    return path_;
}
const RuntimeData& MaterialResource::data() const {
    return data_;
}
const std::vector<MaterialResource::Parameter>& MaterialResource::parameters()
    const {
    return parameters_;
}
const std::vector<MaterialResource::Node>& MaterialResource::nodes() const {
    return nodes_;
}
const std::string& MaterialResource::output() const {
    return output_;
}
sf::Vector2f MaterialResource::maximumVertexOffset() const {
    return maximumVertexOffset_;
}
const std::string& MaterialResource::signature() const {
    return signature_;
}

void MaterialResource::validateParameters(
    const RuntimeData::Map& overrides) const {
    for (const auto& [id, value] : overrides) {
        const auto parameter =
            std::find_if(parameters_.begin(), parameters_.end(),
                         [&id](const Parameter& candidate) {
                             return candidate.id == id;
                         });
        if (parameter == parameters_.end()) {
            throw std::invalid_argument("Unknown material parameter: " + id);
        }
        checkRange(*parameter, value);
    }
}

RuntimeData::Map MaterialResource::resolveParameters(
    const RuntimeData::Map& overrides) const {
    validateParameters(overrides);
    RuntimeData::Map result;
    for (const Parameter& parameter : parameters_) {
        result.emplace(parameter.id, parameter.defaultValue);
    }
    for (const auto& [id, value] : overrides) {
        result[id] = value;
    }
    return result;
}

}  // namespace ludork::engine::materials
