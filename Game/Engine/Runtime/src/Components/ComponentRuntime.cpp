#include <Runtime/Components/ComponentRuntime.hpp>

#include <Components/ComponentRuntimeCache.hpp>
#include <Components/ComponentFieldTarget.hpp>
#include "LuaServices/RuntimeMetadataReferences.hpp"
#include <Runtime/RuntimeReflection.hpp>
#include <Runtime/RuntimeReference.hpp>
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
        raw ? RuntimeReflectionFacade::RuntimeLookupMode::Own
            : RuntimeReflectionFacade::RuntimeLookupMode::Visible);
}

RuntimeValue runtimeGet(const RuntimeValue& value, const std::string& name,
                        bool raw = false) {
    if (std::optional<RuntimeMapView> map = RuntimeValueView(value).map()) {
        const auto iterator = map->find(name);
        return !iterator ? RuntimeValue() : iterator->toValue();
    }
    return runtimeReflection().get(
        ludork::runtime::reference::intern(value), name,
        raw ? RuntimeReflectionFacade::RuntimeLookupMode::Own
            : RuntimeReflectionFacade::RuntimeLookupMode::Visible);
}

void runtimeSet(const RuntimeValue& value, const std::string& name,
                const RuntimeValue& member) {
    runtimeReflection().set(ludork::runtime::reference::intern(value), name,
                            member);
}

bool runtimeHasOwnField(const RuntimeValue& value, const std::string& name) {
    if (value.tag() != RuntimeValue::Tag::Handle &&
        value.tag() != RuntimeValue::Tag::NativeObject) {
        const std::optional<RuntimeMapView> map = value.view().map();
        return map && map->find(name).has_value();
    }
    return ludork::runtime::reference::hasOwnField(
        ludork::runtime::reference::intern(value), RuntimeValue(name));
}

void runtimeSetTyped(const RuntimeValue& value, const std::string& name,
                     const RuntimeValue& member) {
    runtimeReflection().setTyped(ludork::runtime::reference::intern(value),
                                 name, member);
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

void loadComponentTypes(ComponentRuntimeCache::Lease& cache,
                        const RuntimeValue& classValue);
void loadComponentFieldMap(ComponentRuntimeCache::Lease& cache,
                           const RuntimeValue& classValue);

void loadComponentFieldDefaults(ComponentRuntimeCache::Lease& cache,
                                const RuntimeValue& componentType) {
    if (cache.initialized()) {
        return;
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
            defaults[name] =
                cloneComponentFieldValue(componentType, name, defaultValue);
        }
    }
    auto& values =
        cache.descriptor<ComponentRuntimeCache::FieldDefaults>().values;
    values.clear();
    for (const auto& [name, value] : defaults) {
        values.emplace(name, cache.store(value));
    }
    cache.complete();
}

std::vector<std::string> componentFieldNames(
    const RuntimeValue& componentType) {
    ComponentRuntimeCache::Lease cache(
        ComponentRuntimeCache::Kind::FieldDefaults, componentType);
    loadComponentFieldDefaults(cache, componentType);
    std::vector<std::string> names;
    const auto& values =
        cache.descriptor<ComponentRuntimeCache::FieldDefaults>().values;
    names.reserve(values.size());
    for (const auto& [name, slot] : values) {
        names.push_back(name);
    }
    return names;
}

RuntimeValue cloneRuntimeComponentFieldValue(const RuntimeValue& componentType,
                                             const std::string& fieldName,
                                             const RuntimeValue& value) {
    const RuntimeValue metadata =
        typedDataService().resolveAttrMetadata(componentType, fieldName);
    if (metadata.isNil()) {
        return cloneComponentRuntimeValue(value);
    }
    const RuntimeValue type = runtimeGet(metadata, "type");
    const RuntimeValue moduleValue = runtimeGet(metadata, "module");
    const std::string* module = moduleValue.getIf<std::string>();
    const RuntimeValue resolved = typedDataService().resolveRuntimeTypedValue(
        value, type, module == nullptr ? std::string{} : *module);
    return cloneComponentRuntimeValue(resolved);
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

void loadInheritedComponentDefaults(ComponentRuntimeCache::Lease& cache,
                                    const RuntimeValue& classValue,
                                    ComponentRuntimeCache::Lease& types) {
    if (cache.initialized()) {
        return;
    }
    auto& result =
        cache.descriptor<ComponentRuntimeCache::InheritedDefaults>().values;
    result.clear();
    const RuntimeValue::Array mro = runtimeMro(classValue);
    for (const auto& [componentName, typeSlot] :
         types.descriptor<ComponentRuntimeCache::Types>().values) {
        const RuntimeValue componentType = types.value(typeSlot);
        std::unordered_map<std::string, int> componentDefaults;
        std::optional<std::vector<std::string>> fieldNames;
        for (std::size_t index = 1; index < mro.size(); ++index) {
            const RuntimeValue parentValue =
                runtimeGet(mro[index], componentName, true);
            if (parentValue.isNil()) {
                continue;
            }
            if (!fieldNames) {
                fieldNames = componentFieldNames(componentType);
            }
            const RuntimeValue parentComponent =
                componentFromData(componentType, parentValue);
            for (const std::string& fieldName : *fieldNames) {
                if (componentDefaults.contains(fieldName)) {
                    continue;
                }
                const RuntimeValue parent =
                    runtimeGet(parentComponent, fieldName);
                if (!isBlankComponentValue(parent) ||
                    (parent.isNil() &&
                     runtimeHasOwnField(parentComponent, fieldName))) {
                    componentDefaults.emplace(
                        fieldName, cache.store(cloneRuntimeComponentFieldValue(
                                       componentType, fieldName, parent)));
                }
            }
        }
        if (!componentDefaults.empty()) {
            result.emplace(componentName, std::move(componentDefaults));
        }
    }
    cache.complete();
}

void mergeComponentDefaultsImpl(const RuntimeValue& object,
                                const RuntimeValue& objectType,
                                ComponentRuntimeCache::Lease& types) {
    ComponentRuntimeCache::Lease inherited(
        ComponentRuntimeCache::Kind::InheritedDefaults, objectType);
    loadInheritedComponentDefaults(inherited, objectType, types);
    const auto& inheritedValues =
        inherited.descriptor<ComponentRuntimeCache::InheritedDefaults>().values;
    for (const auto& [componentName, typeSlot] :
         types.descriptor<ComponentRuntimeCache::Types>().values) {
        const RuntimeValue componentType = types.value(typeSlot);
        RuntimeValue value = runtimeGet(object, componentName);
        if (value.isNil()) {
            continue;
        }
        if (!runtimeIsInstance(value, componentType)) {
            value = componentFromData(componentType, value);
            runtimeSet(object, componentName, value);
        }
        const auto componentDefaults = inheritedValues.find(componentName);
        if (componentDefaults == inheritedValues.end()) {
            continue;
        }
        for (const auto& [fieldName, slot] : componentDefaults->second) {
            const RuntimeValue current = runtimeGet(value, fieldName);
            if (isBlankComponentValue(current) &&
                !(current.isNil() && runtimeHasOwnField(value, fieldName))) {
                runtimeSetTyped(
                    value, fieldName,
                    cloneRuntimeComponentFieldValue(componentType, fieldName,
                                                    inherited.value(slot)));
            }
        }
    }
}

void normaliseInstanceComponentsImpl(const RuntimeValue& object,
                                     const RuntimeValue& objectType,
                                     ComponentRuntimeCache::Lease& types) {
    for (const auto& [componentName, typeSlot] :
         types.descriptor<ComponentRuntimeCache::Types>().values) {
        const RuntimeValue componentType = types.value(typeSlot);
        RuntimeValue value = runtimeGet(object, componentName);
        if (!value.isNil() && !runtimeIsInstance(value, componentType)) {
            runtimeSet(object, componentName,
                       componentFromData(componentType, value));
        }
    }
    mergeComponentDefaultsImpl(object, objectType, types);
}

ComponentFieldTarget resolveComponentFieldTarget(const RuntimeValue& object,
                                                 const std::string& fieldName) {
    const RuntimeValue objectType = runtimeType(object);
    ComponentRuntimeCache::Lease fields(ComponentRuntimeCache::Kind::FieldMap,
                                        objectType);
    loadComponentFieldMap(fields, objectType);
    const auto& fieldMap =
        fields.descriptor<ComponentRuntimeCache::FieldMap>().values;
    const auto fieldIterator = fieldMap.find(fieldName);
    if (fieldIterator == fieldMap.end()) {
        return {};
    }
    ComponentRuntimeCache::Lease types(ComponentRuntimeCache::Kind::Types,
                                       objectType);
    loadComponentTypes(types, objectType);
    const auto& componentTypes =
        types.descriptor<ComponentRuntimeCache::Types>().values;
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
    const RuntimeValue componentType = types.value(typeIterator->second);
    if (!runtimeIsInstance(component, componentType)) {
        throw std::runtime_error(
            "Component field '" + componentName +
            "' has the wrong type while resolving member '" + fieldName + "'");
    }
    return {std::move(component), componentType, true};
}

void loadComponentTypes(ComponentRuntimeCache::Lease& cache,
                        const RuntimeValue& classValue) {
    if (cache.initialized()) {
        return;
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
    for (const detail::ComponentTypeReference& reference :
         detail::componentTypeReferences(classValue)) {
        RuntimeValue componentType = typedDataService().resolveMetadataType(
            reference.type, reference.module);
        if (isComponentType(componentType)) {
            result[reference.name] = std::move(componentType);
        }
    }
    auto& values = cache.descriptor<ComponentRuntimeCache::Types>().values;
    values.clear();
    for (const auto& [name, value] : result) {
        values.emplace(name, cache.store(value));
    }
    cache.complete();
}

void loadComponentFieldMap(ComponentRuntimeCache::Lease& cache,
                           const RuntimeValue& classValue) {
    if (cache.initialized()) {
        return;
    }
    ComponentRuntimeCache::Lease types(ComponentRuntimeCache::Kind::Types,
                                       classValue);
    loadComponentTypes(types, classValue);
    auto& result = cache.descriptor<ComponentRuntimeCache::FieldMap>().values;
    result.clear();
    for (const auto& [componentName, typeSlot] :
         types.descriptor<ComponentRuntimeCache::Types>().values) {
        for (const std::string& fieldName :
             componentFieldNames(types.value(typeSlot))) {
            result[fieldName] = componentName;
        }
    }
    cache.complete();
}
}  // namespace

RuntimeValue cloneComponentValue(const RuntimeValue& value,
                                 const RuntimeValue& valueType,
                                 const RuntimeValue& declaringModule) {
    RuntimeValue resolved = value;
    TypedDataService& dataValues = typedDataService();
    if (!valueType.isNil()) {
        const std::string* module = declaringModule.getIf<std::string>();
        resolved = dataValues.resolveTypedDataValue(
            resolved, valueType, RuntimeValue::Map{},
            module == nullptr ? std::string() : *module);
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
    ComponentRuntimeCache::Lease cache(ComponentRuntimeCache::Kind::Types,
                                       classValue);
    loadComponentTypes(cache, classValue);
    RuntimeValue::Map result;
    for (const auto& [name, slot] :
         cache.descriptor<ComponentRuntimeCache::Types>().values) {
        result.emplace(name, cache.value(slot));
    }
    return result;
}

RuntimeValue::Map getComponentFieldDefaults(const RuntimeValue& componentType) {
    ComponentRuntimeCache::Lease cache(
        ComponentRuntimeCache::Kind::FieldDefaults, componentType);
    loadComponentFieldDefaults(cache, componentType);
    RuntimeValue::Map result;
    for (const auto& [name, slot] :
         cache.descriptor<ComponentRuntimeCache::FieldDefaults>().values) {
        const RuntimeValue value = cache.value(slot);
        result.emplace(name, cloneComponentRuntimeValue(value));
    }
    return result;
}

std::unordered_map<std::string, std::string> getComponentFieldMap(
    const RuntimeValue& classValue) {
    ComponentRuntimeCache::Lease cache(ComponentRuntimeCache::Kind::FieldMap,
                                       classValue);
    loadComponentFieldMap(cache, classValue);
    return cache.descriptor<ComponentRuntimeCache::FieldMap>().values;
}

RuntimeValue componentFromData(const RuntimeValue& componentType,
                               const RuntimeValue& data) {
    const RuntimeValue& source = data;
    const bool runtimeSource = runtimeIsInstance(source, componentType);
    const bool hasSourceFields =
        runtimeSource || runtimeKind(source) == "table";
    ComponentRuntimeCache::Lease defaults(
        ComponentRuntimeCache::Kind::FieldDefaults, componentType);
    loadComponentFieldDefaults(defaults, componentType);
    RuntimeValue::Map values;
    for (const auto& [name, slot] :
         defaults.descriptor<ComponentRuntimeCache::FieldDefaults>().values) {
        const RuntimeValue supplied =
            hasSourceFields ? runtimeGet(source, name) : RuntimeValue();
        if (runtimeSource || !supplied.isNil()) {
            values.emplace(name, runtimeSource
                                     ? cloneRuntimeComponentFieldValue(
                                           componentType, name, supplied)
                                     : cloneComponentFieldValue(
                                           componentType, name, supplied));
        } else {
            const RuntimeValue defaultValue = defaults.value(slot);
            values.emplace(name, cloneComponentRuntimeValue(defaultValue));
        }
    }
    std::vector<std::string> nilFields;
    for (const auto& [name, value] : values) {
        if (value.isNil()) {
            nilFields.push_back(name);
        }
    }
    const RuntimeValue result = runtimeReflection().construct(
        ludork::runtime::reference::intern(componentType),
        {RuntimeValue(std::move(values))});
    for (const std::string& name : nilFields) {
        runtimeSetTyped(result, name, RuntimeValue());
    }
    return result;
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
        keys = componentFieldNames(runtimeType(value));
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

std::tuple<RuntimeValue, std::optional<std::string>, RuntimeValue>
getComponentFieldTarget(const RuntimeValue& object,
                        const std::string& fieldName) {
    ComponentFieldTarget target =
        resolveComponentFieldTarget(object, fieldName);
    if (!target.found) {
        return {RuntimeValue(), std::nullopt, RuntimeValue()};
    }
    return {std::move(target.component), fieldName,
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
    if (!target.found) {
        return false;
    }
    runtimeSetTyped(target.component, fieldName,
                    cloneRuntimeComponentFieldValue(target.componentType,
                                                    fieldName, value));
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
    ComponentRuntimeCache::Lease types(ComponentRuntimeCache::Kind::Types,
                                       objectType);
    loadComponentTypes(types, objectType);
    mergeComponentDefaultsImpl(object, objectType, types);
}

void normaliseInstanceComponents(const RuntimeValue& object) {
    const RuntimeValue objectType = runtimeType(object);
    ComponentRuntimeCache::Lease types(ComponentRuntimeCache::Kind::Types,
                                       objectType);
    loadComponentTypes(types, objectType);
    normaliseInstanceComponentsImpl(object, objectType, types);
}

RuntimeValue::Array attachInstanceComponents(const RuntimeValue& object) {
    const RuntimeValue objectType = runtimeType(object);
    ComponentRuntimeCache::Lease types(ComponentRuntimeCache::Kind::Types,
                                       objectType);
    loadComponentTypes(types, objectType);
    normaliseInstanceComponentsImpl(object, objectType, types);
    RuntimeValue::Array spawned;
    const RuntimeValue componentBase = resolveRuntimeType("Component");
    for (const auto& [componentName, slot] :
         types.descriptor<ComponentRuntimeCache::Types>().values) {
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
