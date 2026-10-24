#include <Graphics/Materials/MaterialResource.hpp>
#include <Graphics/Materials/MaterialCompiler.hpp>
#include <Graphics/Materials/MaterialGenerated.hpp>
#include "MaterialSchema.hpp"

#include <Runtime/ConcurrentResourceCache.hpp>
#include <Runtime/Data.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <DataFile.hpp>
#include <ReadOnlyFileProvider.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <unordered_set>
#include <utility>

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

ludork::runtime::ConcurrentResourceCache<const MaterialResource, true>&
previewResourceCache() {
    static ludork::runtime::ConcurrentResourceCache<const MaterialResource,
                                                    true>
        cache;
    return cache;
}

std::string previewKey(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    const std::filesystem::path resolved =
        ludork::standard::resolveDataPath(ludork::standard::pathFromUtf8(path));
    const auto status = ludork::standard::readOnlyFileStatus(resolved);
    if (status.handled) {
        return path + '\0' + std::to_string(status.size) + ':' +
               std::to_string(status.modificationTime);
    }
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(resolved, error);
    if (error) {
        return path + '\0' + "missing";
    }
    const auto time = std::filesystem::last_write_time(resolved, error);
    if (error) {
        return path + '\0' + "missing";
    }
    return path + '\0' + std::to_string(size) + ':' +
           std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                              time.time_since_epoch())
                              .count());
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

void appendCanonical(std::string& output, const RuntimeData& value) {
    if (const auto* map = value.getIf<RuntimeData::Map>()) {
        std::vector<std::string> keys;
        keys.reserve(map->size());
        for (const auto& [key, item] : *map) {
            static_cast<void>(item);
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        output += '{';
        for (const std::string& key : keys) {
            output += stringifyJSON(RuntimeData(key));
            output += ':';
            appendCanonical(output, map->at(key));
            output += ',';
        }
        output += '}';
    } else if (const auto* array = value.getIf<RuntimeData::Array>()) {
        output += '[';
        for (const RuntimeData& item : *array) {
            appendCanonical(output, item);
            output += ',';
        }
        output += ']';
    } else {
        output += stringifyJSON(value);
    }
}

std::string sourceSignature(const RuntimeData& value) {
    std::string canonical;
    appendCanonical(canonical, value);
    std::uint64_t fingerprint = 14695981039346656037ull;
    for (const unsigned char byte : canonical) {
        fingerprint = (fingerprint ^ byte) * 1099511628211ull;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << fingerprint;
    return output.str();
}

}  // namespace

std::shared_ptr<const MaterialResource> MaterialResource::load(
    const std::string& path) {
    static_cast<void>(MaterialGenerated::resourceDirectory(path));
    for (auto scope = overrides.rbegin(); scope != overrides.rend(); ++scope) {
        if (const auto found = (*scope)->resources_.find(path);
            found != (*scope)->resources_.end()) {
            return found->second;
        }
        const Overrides& values = (*scope)->values_;
        if (const auto found = values.resources_.find(path);
            found != values.resources_.end()) {
            return found->second;
        }
        if (const auto document = values.documents_.find(path);
            document != values.documents_.end()) {
            const auto resource = fromData(document->second, path);
            values.resources_.emplace(path, resource);
            return resource;
        }
    }
    if (!overrides.empty()) {
        auto& cache = previewResourceCache();
        if (cache.entryCount() >= PreviewResourceCacheCapacity) {
            cache.clear();
        }
        const auto resource = cache.getOrLoad(previewKey(path), [&path]() {
            return fromData(path.empty() ? defaultDocument() : loadData(path),
                            path);
        });
        overrides.back()->resources_.emplace(path, resource);
        return resource;
    }
    return resourceCache().getOrLoad(path, [&path]() {
        const RuntimeData data = MaterialGenerated::loadMetadata(
            MaterialGenerated::resourceDirectory(path) + "/resource.json");
        const RuntimeData::Map& root =
            reader::requireMap(data, "generated material");
        if (text(root, "type") != "materialResource" ||
            reader::requireInt(required(root, "version"),
                               "generated material.version") != 1 ||
            text(root, "generator") != "Ludork Material Export" ||
            text(root, "material") != path) {
            throw std::invalid_argument(
                "Invalid generated material metadata: " + path);
        }
        const auto resource = readMetadata(data, path);
        resource->signature_ = text(root, "signature");
        if (resource->signature_.empty()) {
            throw std::invalid_argument(
                "Generated material signature is empty: " + path);
        }
        return resource;
    });
}

MaterialResource::Overrides::Overrides(RuntimeData::Map documents)
    : documents_(std::move(documents)) {
    for (const auto& [path, data] : documents_) {
        static_cast<void>(data);
        if (path.empty()) {
            throw std::invalid_argument(
                "Material preview override requires a resource path");
        }
    }
}

MaterialResource::ScopedOverrides::ScopedOverrides(const Overrides& values)
    : values_(values) {
    overrides.push_back(this);
}

MaterialResource::ScopedOverrides::~ScopedOverrides() {
    std::erase(overrides, this);
}

std::shared_ptr<MaterialResource> MaterialResource::readMetadata(
    const RuntimeData& data, std::string path) {
    const RuntimeData::Map& root = reader::requireMap(data, "material");
    std::shared_ptr<MaterialResource> result(new MaterialResource());
    result->path_ = std::move(path);
    std::unordered_set<std::string> parameterIds;
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
        result->parameters_.push_back(std::move(parameter));
    }
    const RuntimeData::Map rendering = optionalMap(root, "rendering");
    if (const RuntimeData* offset =
            reader::findValue(rendering, "maxVertexOffset")) {
        schema::validateParameterValue(schema::value_type::Vec2, *offset,
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
    return result;
}

std::shared_ptr<const MaterialResource> MaterialResource::fromData(
    const RuntimeData& data, std::string path) {
    static_cast<void>(MaterialGenerated::resourceDirectory(path));
    const RuntimeData::Map& root = reader::requireMap(data, "material");
    if (text(root, "type") != "material" ||
        reader::requireInt(required(root, "version"), "material.version") !=
            1) {
        throw std::invalid_argument(
            "Material requires type material and version 1");
    }
    const auto result = readMetadata(data, std::move(path));
    result->definition_ = MaterialDefinition::fromData(data);
    RuntimeData::Array parameters;
    for (const RuntimeData& value : reader::requireArray(
             required(root, "parameters"), "material.parameters")) {
        RuntimeData::Map parameter =
            reader::requireMap(value, "material parameter");
        parameter.erase("displayName");
        parameter.erase("group");
        parameters.emplace_back(std::move(parameter));
    }
    result->signature_ = sourceSignature(RuntimeData(RuntimeData::Map{
        {"graph", required(root, "graph")},
        {"parameters", RuntimeData(std::move(parameters))},
        {"rendering", RuntimeData(optionalMap(root, "rendering"))}}));
    static_cast<void>(MaterialCompiler::compile(*result));
    return result;
}

std::shared_ptr<const MaterialResource> MaterialResource::defaultMaterial() {
    return load("");
}

RuntimeData MaterialResource::defaultDocument() {
    return schema::defaultDocument();
}

RuntimeData MaterialResource::nodeRegistry() {
    RuntimeData::Array types;
    RuntimeData::Map defaults;
    RuntimeData::Array fields;
    for (const auto& [id, type, required] :
         {std::tuple{"id", schema::value_type::String, true},
          std::tuple{"displayName", schema::value_type::String, false},
          std::tuple{"type", "parameterType", true},
          std::tuple{"default", "parameterValue", true},
          std::tuple{"group", schema::value_type::String, false},
          std::tuple{"min", schema::value_type::Float, false},
          std::tuple{"max", schema::value_type::Float, false}}) {
        fields.emplace_back(
            RuntimeData::Map{{"id", RuntimeData(id)},
                             {"type", RuntimeData(type)},
                             {"required", RuntimeData(required)}});
    }
    for (const char* type :
         {schema::value_type::Float, schema::value_type::Vec2,
          schema::value_type::Vec3, schema::value_type::Vec4,
          schema::value_type::Color, schema::value_type::Boolean,
          schema::value_type::Texture}) {
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
                RuntimeData(std::string(type) == schema::value_type::Color
                                ? 1.0
                                : 0.0));
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
        {"rangeParameterTypes", RuntimeData(RuntimeData::Array{
                                    RuntimeData(schema::value_type::Float),
                                    RuntimeData(schema::value_type::Vec2),
                                    RuntimeData(schema::value_type::Vec3),
                                    RuntimeData(schema::value_type::Vec4),
                                    RuntimeData(schema::value_type::Color)})},
        {"parameterDefaults", RuntimeData(std::move(defaults))}});
}

void MaterialResource::clearCache() {
    resourceCache().clear();
    previewResourceCache().clear();
    MaterialGenerated::clearCache();
}
const std::string& MaterialResource::path() const {
    return path_;
}
const MaterialDefinition* MaterialResource::definition() const {
    return definition_.get();
}
const std::vector<MaterialResource::Parameter>& MaterialResource::parameters()
    const {
    return parameters_;
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
                         [&parameterId = id](const Parameter& candidate) {
                             return candidate.id == parameterId;
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
