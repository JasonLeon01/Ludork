#include <Gameplay/Components/ComponentRuntime.hpp>

#include <Runtime/RuntimeValueServices.hpp>
#include <Utils/DataValue.hpp>

#include <cstdint>
#include <utility>

namespace ludork::engine::components {
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
    if (value.getIf<RuntimeValue::Array>() != nullptr ||
        value.getIf<RuntimeValue::Map>() != nullptr) {
        return "table";
    }
    return ludork::engine::runtime_services::invokeString("reflect.kind",
                                                          {value});
}

RuntimeValue runtimeType(const RuntimeValue& value) {
    return ludork::engine::runtime_services::invokeFirst("reflect.type",
                                                         {value});
}

bool runtimeIsSubclass(const RuntimeValue& value, const RuntimeValue& base) {
    return ludork::engine::runtime_services::invokeBool("reflect.isSubclass",
                                                        {value, base});
}

bool runtimeIsInstance(const RuntimeValue& value, const RuntimeValue& type) {
    return ludork::engine::runtime_services::invokeBool("reflect.isInstance",
                                                        {value, type});
}

RuntimeValue::Array runtimeMro(const RuntimeValue& type) {
    RuntimeValue value =
        ludork::engine::runtime_services::invokeFirst("reflect.mro", {type});
    if (const RuntimeValue::Array* mro = value.getIf<RuntimeValue::Array>()) {
        return *mro;
    }
    return runtimeKind(type) == "table" ? RuntimeValue::Array{type}
                                        : RuntimeValue::Array{};
}

std::vector<std::string> runtimeKeys(const RuntimeValue& value, bool raw) {
    if (value.isNil()) {
        return {};
    }
    if (const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>()) {
        std::vector<std::string> result;
        result.reserve(map->size());
        for (const auto& [name, _] : *map) {
            result.push_back(name);
        }
        return result;
    }
    RuntimeValue keys = ludork::engine::runtime_services::invokeFirst(
        raw ? "reflect.rawKeys" : "reflect.keys", {value});
    const RuntimeValue::Array* array = keys.getIf<RuntimeValue::Array>();
    if (array == nullptr) {
        return {};
    }
    std::vector<std::string> result;
    result.reserve(array->size());
    for (const RuntimeValue& key : *array) {
        if (const std::string* name = key.getIf<std::string>()) {
            result.push_back(*name);
        }
    }
    return result;
}

RuntimeValue runtimeGet(const RuntimeValue& value, const std::string& name,
                        bool raw = false) {
    if (const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>()) {
        const auto iterator = map->find(name);
        return iterator == map->end() ? RuntimeValue() : iterator->second;
    }
    return ludork::engine::runtime_services::invokeFirst(
        raw ? "reflect.rawGet" : "reflect.get", {value, RuntimeValue(name)});
}

void runtimeSet(const RuntimeValue& value, const std::string& name,
                const RuntimeValue& member) {
    resolveRuntime("reflect.set", {value, RuntimeValue(name), member});
}

RuntimeValue cloneComponentRuntimeValue(const RuntimeValue& value) {
    if (const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>()) {
        RuntimeValue::Array result;
        result.reserve(array->size());
        for (const RuntimeValue& item : *array) {
            result.push_back(cloneComponentRuntimeValue(item));
        }
        return RuntimeValue(std::move(result));
    }
    if (const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>()) {
        RuntimeValue::Map result;
        result.reserve(map->size());
        for (const auto& [key, item] : *map) {
            result.emplace(key, cloneComponentRuntimeValue(item));
        }
        return RuntimeValue(std::move(result));
    }
    if (value.getIf<RuntimeValue::Object>() != nullptr ||
        value.getIf<RuntimeIdentityPtr>() != nullptr) {
        RuntimeValue cloned = ludork::engine::runtime_services::invokeFirst(
            "reflect.clone", {value});
        return cloned.isNil() && !value.isNil() ? value : cloned;
    }
    return value;
}

RuntimeValue::Map cloneRuntimeMap(const RuntimeValue::Map& values) {
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
    return dataValueService().resolveMetadataType(
        RuntimeValue(std::move(typeReference)));
}

RuntimeValue::Map runtimeStringMap(const RuntimeValue& value) {
    if (const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>()) {
        return *map;
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
    const RuntimeValue cached = ludork::engine::runtime_services::invokeFirst(
        "components.cacheGet",
        {RuntimeValue(std::string("inheritedDefaults")), classValue});
    if (const RuntimeValue::Map* defaults = cached.getIf<RuntimeValue::Map>()) {
        return *defaults;
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
    resolveRuntime("components.cacheSet",
                   {RuntimeValue(std::string("inheritedDefaults")), classValue,
                    RuntimeValue(result)});
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
    RuntimeValue component = runtimeGet(object, fieldIterator->second);
    if (!runtimeIsInstance(component, typeIterator->second)) {
        component = componentFromData(typeIterator->second, component);
        runtimeSet(object, fieldIterator->second, component);
    }
    return {std::move(component), typeIterator->second, true};
}

}  // namespace

RuntimeValue cloneComponentValue(const RuntimeValue& value,
                                 const RuntimeValue& valueType,
                                 const RuntimeValue& declaringModule) {
    RuntimeValue resolved = value;
    DataValueService& dataValues = dataValueService();
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
        dataValueService().resolveAttrMetadata(componentType, fieldName);
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
    const RuntimeValue cached = ludork::engine::runtime_services::invokeFirst(
        "components.cacheGet",
        {RuntimeValue(std::string("types")), classValue});
    if (const RuntimeValue::Map* types = cached.getIf<RuntimeValue::Map>()) {
        return *types;
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
        dataValueService().getAttrMetadata(classValue);
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
        RuntimeValue componentType = dataValueService().resolveMetadataType(
            typeReference, module == nullptr ? std::string() : *module);
        if (isComponentType(componentType)) {
            result[name] = std::move(componentType);
        }
    }
    resolveRuntime("components.cacheSet", {RuntimeValue(std::string("types")),
                                           classValue, RuntimeValue(result)});
    return result;
}

RuntimeValue::Map getComponentFieldDefaults(const RuntimeValue& componentType) {
    const RuntimeValue cached = ludork::engine::runtime_services::invokeFirst(
        "components.cacheGet",
        {RuntimeValue(std::string("fieldDefaults")), componentType});
    if (const RuntimeValue::Map* defaults = cached.getIf<RuntimeValue::Map>()) {
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
        dataValueService().getAttrMetadata(componentType);
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
    resolveRuntime("components.cacheSet",
                   {RuntimeValue(std::string("fieldDefaults")), componentType,
                    RuntimeValue(defaults)});
    return cloneRuntimeMap(defaults);
}

std::unordered_map<std::string, std::string> getComponentFieldMap(
    const RuntimeValue& classValue) {
    const RuntimeValue cached = ludork::engine::runtime_services::invokeFirst(
        "components.cacheGet",
        {RuntimeValue(std::string("fieldMap")), classValue});
    if (cached.getIf<RuntimeValue::Map>() != nullptr) {
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
    resolveRuntime("components.cacheSet",
                   {RuntimeValue(std::string("fieldMap")), classValue,
                    RuntimeValue(std::move(cachedMap))});
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
    return ludork::engine::runtime_services::invokeFirst(
        "reflect.construct", {componentType, RuntimeValue(std::move(values))});
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
            result[key] = cloneComponentRuntimeValue(runtimeGet(value, key));
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
    if (const RuntimeValue::Array* array = value.getIf<RuntimeValue::Array>()) {
        return array->empty();
    }
    if (const RuntimeValue::Map* map = value.getIf<RuntimeValue::Map>()) {
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
        const RuntimeValue::Map* componentDefaults =
            inheritedIterator->second.getIf<RuntimeValue::Map>();
        if (componentDefaults == nullptr) {
            continue;
        }
        for (const auto& [fieldName, parent] : *componentDefaults) {
            const RuntimeValue current = runtimeGet(value, fieldName);
            if (isBlankComponentValue(current)) {
                runtimeSet(
                    value, fieldName,
                    cloneComponentFieldValue(componentType, fieldName, parent));
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
        const std::vector<RuntimeValue> results = resolveRuntime(
            "reflect.call", {component, RuntimeValue(std::string("onAttach")),
                             RuntimeValue(RuntimeValue::Array{object})});
        if (results.empty()) {
            continue;
        }
        if (const RuntimeValue::Array* actors =
                results.front().getIf<RuntimeValue::Array>()) {
            spawned.insert(spawned.end(), actors->begin(), actors->end());
        }
    }
    return spawned;
}

}  // namespace ludork::engine::components
