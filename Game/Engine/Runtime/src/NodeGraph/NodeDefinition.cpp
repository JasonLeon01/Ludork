#include <Runtime/NodeGraph/NodeDefinition.hpp>

#include <Runtime/TypedDataService.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

std::optional<RuntimeValueView> mapValue(RuntimeMapView map,
                                         const std::string& name) {
    return map.find(name);
}

std::optional<RuntimeMapView> asMap(
    const std::optional<RuntimeValueView>& value) {
    return !value ? std::nullopt : RuntimeValueView(*value).map();
}

std::optional<RuntimeArrayView> asArray(
    const std::optional<RuntimeValueView>& value) {
    return !value ? std::nullopt : RuntimeValueView(*value).array();
}

std::string stringValue(const std::optional<RuntimeValueView>& value,
                        const std::string& fallback = std::string()) {
    if (!value) {
        return fallback;
    }
    const std::string* text = value->getIf<std::string>();
    return text == nullptr ? fallback : *text;
}

bool boolValue(const std::optional<RuntimeValueView>& value,
               bool fallback = false) {
    if (!value) {
        return fallback;
    }
    const bool* flag = value->getIf<bool>();
    return flag == nullptr ? fallback : *flag;
}

RuntimeIdentityPtr identityValue(const std::optional<RuntimeValueView>& value) {
    if (!value) {
        return nullptr;
    }
    const RuntimeHandle* identity = value->getIf<RuntimeHandle>();
    return identity == nullptr ? nullptr : identity->identity();
}

RuntimeValue::Array singleOrArray(
    const std::optional<RuntimeValueView>& value) {
    if (!value) {
        return {};
    }
    if (std::optional<RuntimeArrayView> array =
            RuntimeValueView(*value).array()) {
        return array->toArray();
    }
    return {value->toValue()};
}

std::vector<NodeNamedValues> parseNamedValues(
    const std::optional<RuntimeValueView>& value) {
    std::vector<NodeNamedValues> result;
    std::optional<RuntimeArrayView> entries = asArray(value);
    if (!entries) {
        return result;
    }
    result.reserve(entries->size());
    for (RuntimeValueView entryValue : *entries) {
        std::optional<RuntimeMapView> entry =
            RuntimeValueView(entryValue).map();
        if (!entry) {
            continue;
        }
        const std::string name = stringValue(mapValue(*entry, "name"));
        if (name.empty()) {
            throw std::runtime_error("Node metadata pin is missing its name");
        }
        result.push_back(
            NodeNamedValues{name, singleOrArray(mapValue(*entry, "values"))});
    }
    return result;
}

}  // namespace

NodeDefinition::CompiledParameter::CompiledParameter(std::string parameterName,
                                                     RuntimeValue type)
    : name(std::move(parameterName)), type_(std::move(type)) {}

const ludork::runtime::TypeSchema& NodeDefinition::CompiledParameter::schema()
    const {
    if (!schema_) {
        schema_ = typedDataService().compileType(type_);
    }
    return *schema_;
}

const RuntimeValue& NodeDefinition::CompiledParameter::typeReference() const {
    return type_;
}

NodeDefinition::NodeDefinition(RuntimeIdentityPtr function)
    : callable(std::move(function)) {}

NodeDefinition::NodeDefinition(const RuntimeValue& resolvedDefinition) {
    const std::optional<RuntimeMapView> descriptor =
        resolvedDefinition.view().map();
    if (!descriptor) {
        throw std::runtime_error("Node resolved definition must be a map");
    }
    callable = identityValue(mapValue(*descriptor, "callable"));
    declaringModule = stringValue(mapValue(*descriptor, "declaringModule"));
    displayName = stringValue(mapValue(*descriptor, "displayName"));
    const std::optional<RuntimeValueView> member =
        mapValue(*descriptor, "memberMeta");
    parseMemberMetadata(member.value_or(RuntimeValueView()));
    if (parameters.empty()) {
        if (const std::optional<RuntimeArrayView> names =
                asArray(mapValue(*descriptor, "paramNames"))) {
            for (RuntimeValueView value : *names) {
                if (const std::string* name = value.getIf<std::string>()) {
                    parameters.emplace_back(*name,
                                            RuntimeValue(std::string("any")));
                }
            }
        }
    }
    hasSelfParameter = metadata.kind == "function" && !parameters.empty() &&
                       parameters.front().name == "self";
}

void NodeDefinition::parseMemberMetadata(RuntimeValueView metadataValue) {
    const std::optional<RuntimeMapView> value = metadataValue.map();
    if (!value) {
        return;
    }
    const std::optional<RuntimeMapView> parameterTypes =
        asMap(mapValue(*value, "parameterTypes"));
    const std::optional<RuntimeArrayView> orderedParameters =
        asArray(mapValue(*value, "parameters"));
    if (orderedParameters) {
        for (RuntimeValueView parameterValue : *orderedParameters) {
            std::string name;
            RuntimeValue type(std::string("any"));
            if (const std::string* parameterName =
                    parameterValue.getIf<std::string>()) {
                name = *parameterName;
                if (parameterTypes) {
                    if (const std::optional<RuntimeValueView> parameterType =
                            parameterTypes->find(name)) {
                        type = parameterType->toValue();
                    }
                }
            } else {
                const std::optional<RuntimeMapView> entry =
                    parameterValue.map();
                if (!entry) {
                    continue;
                }
                name = stringValue(mapValue(*entry, "name"));
                if (name.empty()) {
                    throw std::runtime_error(
                        "parameters order contains an unknown key");
                }
                if (const std::optional<RuntimeValueView> parameterType =
                        mapValue(*entry, "type")) {
                    type = parameterType->toValue();
                }
            }
            if (std::any_of(parameters.begin(), parameters.end(),
                            [&name](const CompiledParameter& parameter) {
                                return parameter.name == name;
                            })) {
                throw std::runtime_error(
                    "parameters order contains duplicate key '" + name + "'");
            }
            parameters.emplace_back(std::move(name), std::move(type));
        }
    }
    std::optional<RuntimeArrayView> defaults =
        asArray(mapValue(*value, "defaults"));
    if (!defaults) {
        defaults = asArray(mapValue(*value, "default"));
    }
    if (defaults) {
        const std::size_t count = std::min(defaults->size(), parameters.size());
        for (std::size_t index = 0; index < count; ++index) {
            if (!(*defaults)[index].isNil()) {
                parameters[index].defaultValue = (*defaults)[index].toValue();
            }
        }
    }
    metadata.execSplits = parseNamedValues(mapValue(*value, "execSplit"));
    metadata.latentStates = parseNamedValues(mapValue(*value, "latentStates"));
    metadata.latent = boolValue(mapValue(*value, "latent"));
    metadata.loop = boolValue(mapValue(*value, "loop"));
    metadata.pure = boolValue(mapValue(*value, "pure"));
    metadata.loopNode = stringValue(mapValue(*value, "loopNode"));
    metadata.kind = stringValue(mapValue(*value, "kind"));
}
