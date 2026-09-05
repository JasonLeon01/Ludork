#include <Runtime/Components/ComponentRuntime.hpp>

#include <Components/ComponentRuntimeCache.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include <Runtime/TypedDataService.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ludork::runtime::components {
namespace {

std::string runtimeKind(const RuntimeValue& value) {
    if (value.isNil()) {
        return "nil";
    }
    if (value.getIf<bool>() != nullptr) {
        return "boolean";
    }
    if (value.getIf<std::int64_t>() != nullptr ||
        value.getIf<double>() != nullptr) {
        return "number";
    }
    if (value.getIf<std::string>() != nullptr) {
        return "string";
    }
    if (RuntimeValueView(value).array().has_value() ||
        RuntimeValueView(value).map().has_value()) {
        return "table";
    }
    return runtimeReflection().kind(value);
}

RuntimeValue runtimeType(const RuntimeValue& value) {
    return runtimeReflection().typeOf(value);
}

bool runtimeIsSubclass(const RuntimeValue& value, const RuntimeValue& base) {
    return runtimeReflection().isSubclass(value, base);
}

bool runtimeIsInstance(const RuntimeValue& value, const RuntimeValue& type) {
    return runtimeReflection().isInstance(value, type);
}

RuntimeValue::Array runtimeMro(const RuntimeValue& type) {
    RuntimeValue::Array value =
        runtimeReflection().mro(ludork::runtime::reference::intern(type));
    if (!value.empty()) {
        return value;
    }
    return runtimeKind(type) == "table" ? RuntimeValue::Array{type}
                                        : RuntimeValue::Array{};
}

std::vector<std::string> runtimeKeys(const RuntimeValue& value, bool raw) {
    if (value.isNil()) {
        return {};
    }
    if (std::optional<RuntimeMapView> map = RuntimeValueView(value).map()) {
        std::vector<std::string> result;
        result.reserve(map->size());
        for (const auto& [name, _] : *map) {
            result.push_back(name);
        }
        return result;
    }
    return runtimeReflection().keys(
        ludork::runtime::reference::intern(value),
        raw ? RuntimeLookupMode::Own : RuntimeLookupMode::Visible);
}

RuntimeValue runtimeGet(const RuntimeValue& value, const std::string& name,
                        bool raw = false) {
    if (std::optional<RuntimeMapView> map = RuntimeValueView(value).map()) {
        const auto iterator = map->find(name);
        return !iterator ? RuntimeValue() : iterator->toValue();
    }
    return runtimeReflection().get(
        ludork::runtime::reference::intern(value), name,
        raw ? RuntimeLookupMode::Own : RuntimeLookupMode::Visible);
}

void runtimeSet(const RuntimeValue& value, const std::string& name,
                const RuntimeValue& member) {
    runtimeReflection().set(ludork::runtime::reference::intern(value), name,
                            member);
}

RuntimeValue cloneComponentRuntimeValue(RuntimeValueView value) {
    if (std::optional<RuntimeArrayView> array =
            RuntimeValueView(value).array()) {
        RuntimeValue::Array result;
        result.reserve(array->size());
        for (RuntimeValueView item : *array) {
            result.push_back(cloneComponentRuntimeValue(item));
        }
        return RuntimeValue(std::move(result));
    }
    if (std::optional<RuntimeMapView> map = RuntimeValueView(value).map()) {
        RuntimeValue::Map result;
        result.reserve(map->size());
        for (const auto& [key, item] : *map) {
            result.emplace(key, cloneComponentRuntimeValue(item));
        }
        return RuntimeValue(std::move(result));
    }
    if (value.getIf<RuntimeValue::Object>() != nullptr ||
        value.getIf<RuntimeHandle>() != nullptr) {
        RuntimeValue cloned = runtimeReflection().clone(value.toValue());
        return cloned.isNil() && !value.isNil() ? value.toValue() : cloned;
    }
    return value.toValue();
}

RuntimeValue::Map cloneRuntimeMap(RuntimeMapView values) {
    RuntimeValue::Map result;
    result.reserve(values.size());
    for (const auto& [name, value] : values) {
        result.emplace(name, cloneComponentRuntimeValue(value));
    }
    return result;
}

RuntimeValue resolveRuntimeType(const std::string& name) {
    RuntimeValue::Array typeReference{
        RuntimeValue(std::string("Engine")),
        RuntimeValue(name),
    };
    return typedDataService().resolveMetadataType(
        RuntimeValue(std::move(typeReference)));
}

RuntimeValue::Map runtimeStringMap(const RuntimeValue& value) {
    if (std::optional<RuntimeMapView> map = RuntimeValueView(value).map()) {
        return map->toMap();
    }
    RuntimeValue::Map result;
    for (const std::string& name : runtimeKeys(value, false)) {
        result.emplace(name, runtimeGet(value, name));
    }
    return result;
}

std::unordered_map<std::string, std::string> componentFieldMapFromValue(
    const RuntimeValue& value) {
    std::unordered_map<std::string, std::string> result;
    const RuntimeValue::Map values = runtimeStringMap(value);
    result.reserve(values.size());
    for (const auto& [fieldName, componentValue] : values) {
        if (const std::string* componentName =
                componentValue.getIf<std::string>()) {
            result.emplace(fieldName, *componentName);
        }
    }
    return result;
}

RuntimeValue::Map inheritedComponentDefaults(const RuntimeValue& classValue) {
    const RuntimeValue cached = componentRuntimeCache().get(
        ComponentRuntimeCacheKind::InheritedDefaults, classValue);
    if (std::optional<RuntimeMapView> defaults =
            RuntimeValueView(cached).map()) {
        return defaults->toMap();
    }
    RuntimeValue::Map result;
    const RuntimeValue::Array mro = runtimeMro(classValue);
    for (const auto& [componentName, componentType] :
         getComponentTypes(classValue)) {
        RuntimeValue::Map componentDefaults;
        const RuntimeValue::Map fieldDefaults =
            getComponentFieldDefaults(componentType);
        for (std::size_t index = 1; index < mro.size(); ++index) {
            const RuntimeValue parentValue =
                runtimeGet(mro[index], componentName, true);
            if (parentValue.isNil()) {
                continue;
            }
            const RuntimeValue parentComponent =
                componentFromData(componentType, parentValue);
            for (const auto& [fieldName, _] : fieldDefaults) {
                if (componentDefaults.contains(fieldName)) {
                    continue;
                }
                const RuntimeValue parent =
                    runtimeGet(parentComponent, fieldName);
                if (!isBlankComponentValue(parent)) {
                    componentDefaults.emplace(
                        fieldName, cloneComponentFieldValue(componentType,
                                                            fieldName, parent));
                }
            }
        }
        if (!componentDefaults.empty()) {
            result.emplace(componentName,
                           RuntimeValue(std::move(componentDefaults)));
        }
    }
    componentRuntimeCache().set(ComponentRuntimeCacheKind::InheritedDefaults,
                                classValue, RuntimeValue(result));
    return result;
}

struct ComponentFieldTarget {
    RuntimeValue component;
    RuntimeValue componentType;
    bool found = false;
};

ComponentFieldTarget resolveComponentFieldTarget(const RuntimeValue& object,
                                                 const std::string& fieldName) {
    const RuntimeValue objectType = runtimeType(object);
    const std::unordered_map<std::string, std::string> fieldMap =
        getComponentFieldMap(objectType);
    const auto fieldIterator = fieldMap.find(fieldName);
    if (fieldIterator == fieldMap.end()) {
        return {};
    }
    const RuntimeValue::Map componentTypes = getComponentTypes(objectType);
    const auto typeIterator = componentTypes.find(fieldIterator->second);
    if (typeIterator == componentTypes.end()) {
        return {};
    }
    const std::string& componentName = fieldIterator->second;
    RuntimeValue component = runtimeGet(object, componentName);
    if (component.isNil()) {
        throw std::runtime_error("Component field '" + componentName +
                                 "' is missing while resolving member '" +
                                 fieldName + "'");
    }
    if (!runtimeIsInstance(component, typeIterator->second)) {
        throw std::runtime_error(
            "Component field '" + componentName +
            "' has the wrong type while resolving member '" + fieldName + "'");
    }
    return {std::move(component), typeIterator->second, true};
}

}  // namespace

RuntimeValue cloneComponentValue(const RuntimeValue& value,
                                 const RuntimeValue& valueType,
                                 const RuntimeValue& declaringModule) {
    RuntimeValue resolved = value;
    TypedDataService& dataValues = typedDataService();
    if (value.getIf<std::string>() != nullptr &&
        dataValues.shouldEvalValueType(valueType)) {
        resolved = dataValues.evalDataExpression(value);
    }
    if (!valueType.isNil()) {
        const std::string* typeName = valueType.getIf<std::string>();
        if (typeName == nullptr || *typeName != "any") {
            const std::string* module = declaringModule.getIf<std::string>();
            resolved = dataValues.resolveTypedDataValue(
                resolved, valueType, RuntimeValue::Map{},
                module == nullptr ? std::string() : *module);
        }
    }
    return cloneComponentRuntimeValue(resolved);
}

RuntimeValue cloneComponentFieldValue(const RuntimeValue& componentType,
                                      const std::string& fieldName,
                                      const RuntimeValue& value) {
    const RuntimeValue metadata =
        typedDataService().resolveAttrMetadata(componentType, fieldName);
    if (metadata.isNil()) {
        return cloneComponentRuntimeValue(value);
    }
    return cloneComponentValue(value, runtimeGet(metadata, "type"),
                               runtimeGet(metadata, "module"));
}

bool isComponentType(const RuntimeValue& value) {
    if (runtimeKind(value) != "table") {
        return false;
    }
    const RuntimeValue base = resolveRuntimeType("Component");
    return !base.isNil() && runtimeIsSubclass(value, base);
}

RuntimeValue::Map getComponentTypes(const RuntimeValue& classValue) {
    if (runtimeKind(classValue) != "table") {
        return {};
    }
    const RuntimeValue cached = componentRuntimeCache().get(
        ComponentRuntimeCacheKind::Types, classValue);
    if (std::optional<RuntimeMapView> types = RuntimeValueView(cached).map()) {
        return types->toMap();
    }
    RuntimeValue::Map result;
    RuntimeValue::Array mro = runtimeMro(classValue);
    for (auto iterator = mro.rbegin(); iterator != mro.rend(); ++iterator) {
        const RuntimeValue declared =
            runtimeGet(*iterator, "_componentTypes", true);
        for (auto& [name, componentType] : runtimeStringMap(declared)) {
            if (isComponentType(componentType)) {
                result[name] = std::move(componentType);
            }
        }
    }
    const RuntimeValue metadata =
        typedDataService().getAttrMetadata(classValue);
    for (const std::string& name : runtimeKeys(metadata, false)) {
        const RuntimeValue descriptor = runtimeGet(metadata, name);
        const RuntimeValue componentValue = runtimeGet(descriptor, "component");
        const bool* component = componentValue.getIf<bool>();
        if (component == nullptr || !*component) {
            continue;
        }
        const RuntimeValue typeReference = runtimeGet(descriptor, "type");
        const RuntimeValue moduleValue = runtimeGet(descriptor, "module");
        const std::string* module = moduleValue.getIf<std::string>();
        RuntimeValue componentType = typedDataService().resolveMetadataType(
            typeReference, module == nullptr ? std::string() : *module);
        if (isComponentType(componentType)) {
            result[name] = std::move(componentType);
        }
    }
    componentRuntimeCache().set(ComponentRuntimeCacheKind::Types, classValue,
                                RuntimeValue(result));
    return result;
}

RuntimeValue::Map getComponentFieldDefaults(const RuntimeValue& componentType) {
    const RuntimeValue cached = componentRuntimeCache().get(
        ComponentRuntimeCacheKind::FieldDefaults, componentType);
    if (std::optional<RuntimeMapView> defaults =
            RuntimeValueView(cached).map()) {
        return cloneRuntimeMap(*defaults);
    }
    RuntimeValue::Map defaults;
    RuntimeValue::Array mro = runtimeMro(componentType);
    for (auto iterator = mro.rbegin(); iterator != mro.rend(); ++iterator) {
        for (const std::string& name : runtimeKeys(*iterator, true)) {
            const RuntimeValue value = runtimeGet(*iterator, name, true);
            if (!name.starts_with("__") && name != "new" &&
                runtimeKind(value) != "function") {
                defaults[name] = cloneComponentRuntimeValue(value);
            }
        }
    }
    const RuntimeValue metadata =
        typedDataService().getAttrMetadata(componentType);
    for (const std::string& name : runtimeKeys(metadata, false)) {
        const RuntimeValue descriptor = runtimeGet(metadata, name);
        const RuntimeValue member = runtimeGet(descriptor, "metadata");
        const RuntimeValue defaultValue = runtimeGet(member, "default");
        if (!defaultValue.isNil()) {
            defaults[name] = defaultValue;
        }
    }
    for (auto& [name, value] : defaults) {
        value = cloneComponentFieldValue(componentType, name, value);
    }
    componentRuntimeCache().set(ComponentRuntimeCacheKind::FieldDefaults,
                                componentType, RuntimeValue(defaults));
    return cloneRuntimeMap(defaults);
}

std::unordered_map<std::string, std::string> getComponentFieldMap(
    const RuntimeValue& classValue) {
    const RuntimeValue cached = componentRuntimeCache().get(
        ComponentRuntimeCacheKind::FieldMap, classValue);
    if (RuntimeValueView(cached).map().has_value()) {
        return componentFieldMapFromValue(cached);
    }
    std::unordered_map<std::string, std::string> result;
    for (const auto& [componentName, componentType] :
         getComponentTypes(classValue)) {
        for (const auto& [fieldName, _] :
             getComponentFieldDefaults(componentType)) {
            result[fieldName] = componentName;
        }
    }
    RuntimeValue::Map cachedMap;
    cachedMap.reserve(result.size());
    for (const auto& [fieldName, componentName] : result) {
        cachedMap.emplace(fieldName, RuntimeValue(componentName));
    }
    componentRuntimeCache().set(ComponentRuntimeCacheKind::FieldMap, classValue,
                                RuntimeValue(std::move(cachedMap)));
    return result;
}

RuntimeValue componentFromData(const RuntimeValue& componentType,
                               const RuntimeValue& data) {
    RuntimeValue source = data;
    if (runtimeIsInstance(source, componentType)) {
        source = RuntimeValue(componentToData(source));
    }
    RuntimeValue::Map values = getComponentFieldDefaults(componentType);
    if (runtimeKind(source) == "table") {
        for (auto& [name, value] : values) {
            const RuntimeValue supplied = runtimeGet(source, name);
            if (!supplied.isNil()) {
                value = cloneComponentFieldValue(componentType, name, supplied);
            }
        }
    }
    return runtimeReflection().construct(
        ludork::runtime::reference::intern(componentType),
        {RuntimeValue(std::move(values))});
}

RuntimeValue::Map componentToData(const RuntimeValue& value) {
    const std::string kind = runtimeKind(value);
    std::vector<std::string> keys;
    if (kind == "table") {
        keys = runtimeKeys(value, false);
    } else if (kind == "userdata") {
        const RuntimeValue componentBase = resolveRuntimeType("Component");
        if (!runtimeIsInstance(value, componentBase)) {
            return {};
        }
        for (const auto& [name, _] :
             getComponentFieldDefaults(runtimeType(value))) {
            keys.push_back(name);
        }
    } else {
        return {};
    }
    RuntimeValue::Map result;
    for (const std::string& key : keys) {
        if (!key.empty() && key.front() != '_') {
            const RuntimeValue member = runtimeGet(value, key);
            result[key] = cloneComponentRuntimeValue(member);
        }
    }
    return result;
}

std::tuple<RuntimeValue, RuntimeValue, RuntimeValue> getComponentFieldTarget(
    const RuntimeValue& object, const std::string& fieldName) {
    ComponentFieldTarget target =
        resolveComponentFieldTarget(object, fieldName);
    if (!target.found) {
        return {RuntimeValue(), RuntimeValue(), RuntimeValue()};
    }
    return {std::move(target.component), RuntimeValue(fieldName),
            std::move(target.componentType)};
}

RuntimeValue getComponentFieldValue(const RuntimeValue& object,
                                    const std::string& fieldName,
                                    const RuntimeValue& defaultValue) {
    ComponentFieldTarget target =
        resolveComponentFieldTarget(object, fieldName);
    if (!target.found) {
        return defaultValue;
    }
    const RuntimeValue value = runtimeGet(target.component, fieldName);
    return value.isNil() ? defaultValue : value;
}

bool setComponentFieldValue(const RuntimeValue& object,
                            const std::string& fieldName,
                            const RuntimeValue& value) {
    ComponentFieldTarget target =
        resolveComponentFieldTarget(object, fieldName);
    if (!target.found || runtimeGet(target.component, fieldName).isNil()) {
        return false;
    }
    runtimeSet(
        target.component, fieldName,
        cloneComponentFieldValue(target.componentType, fieldName, value));
    return true;
}

bool isBlankComponentValue(const RuntimeValue& value) {
    if (value.isNil()) {
        return true;
    }
    if (const std::string* text = value.getIf<std::string>()) {
        return text->empty();
    }
    if (std::optional<RuntimeArrayView> array =
            RuntimeValueView(value).array()) {
        return array->empty();
    }
    if (std::optional<RuntimeMapView> map = RuntimeValueView(value).map()) {
        return map->empty();
    }
    return runtimeKind(value) == "table" && runtimeKeys(value, false).empty();
}

void mergeComponentDefaults(const RuntimeValue& object) {
    const RuntimeValue objectType = runtimeType(object);
    const RuntimeValue::Map inheritedDefaults =
        inheritedComponentDefaults(objectType);
    for (const auto& [componentName, componentType] :
         getComponentTypes(objectType)) {
        RuntimeValue value = runtimeGet(object, componentName);
        if (value.isNil()) {
            continue;
        }
        if (!runtimeIsInstance(value, componentType)) {
            value = componentFromData(componentType, value);
            runtimeSet(object, componentName, value);
        }
        const auto inheritedIterator = inheritedDefaults.find(componentName);
        if (inheritedIterator == inheritedDefaults.end()) {
            continue;
        }
        std::optional<RuntimeMapView> componentDefaults =
            RuntimeValueView(inheritedIterator->second).map();
        if (!componentDefaults) {
            continue;
        }
        for (const auto& [fieldName, parent] : *componentDefaults) {
            const RuntimeValue current = runtimeGet(value, fieldName);
            if (isBlankComponentValue(current)) {
                runtimeSet(value, fieldName,
                           cloneComponentFieldValue(componentType, fieldName,
                                                    parent.toValue()));
            }
        }
    }
}

void normaliseInstanceComponents(const RuntimeValue& object) {
    const RuntimeValue objectType = runtimeType(object);
    for (const auto& [componentName, componentType] :
         getComponentTypes(objectType)) {
        RuntimeValue value = runtimeGet(object, componentName);
        if (!value.isNil() && !runtimeIsInstance(value, componentType)) {
            runtimeSet(object, componentName,
                       componentFromData(componentType, value));
        }
    }
    mergeComponentDefaults(object);
}

RuntimeValue::Array attachInstanceComponents(const RuntimeValue& object) {
    normaliseInstanceComponents(object);
    RuntimeValue::Array spawned;
    const RuntimeValue componentBase = resolveRuntimeType("Component");
    for (const auto& [componentName, _] :
         getComponentTypes(runtimeType(object))) {
        const RuntimeValue component = runtimeGet(object, componentName);
        if (!runtimeIsInstance(component, componentBase)) {
            continue;
        }
        const RuntimeValue::Array results = runtimeReflection().call(
            ludork::runtime::reference::intern(component), "onAttach",
            {object});
        if (results.empty()) {
            continue;
        }
        if (std::optional<RuntimeArrayView> actors =
                RuntimeValueView(results.front()).array()) {
            for (RuntimeValueView actor : *actors) {
                spawned.push_back(actor.toValue());
            }
        }
    }
    return spawned;
}

}  // namespace ludork::runtime::components
