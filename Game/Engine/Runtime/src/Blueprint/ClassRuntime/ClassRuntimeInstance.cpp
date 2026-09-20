#include <Runtime/RuntimeProviderFacade.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include "ClassRuntimeInternal.hpp"
#include "LuaServices/RuntimeMetadataReferences.hpp"
#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeReferenceConversion.hpp"
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <new>

extern "C" {
#include <lauxlib.h>
}

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <Runtime/TypedDataService.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::runtime::class_runtime_detail {

using namespace ludork::runtime::reference;

namespace {

constexpr const char* configReferenceMetatable =
    "Ludork.Runtime.ClassConfigReferences";

int destroyConfigReferences(lua_State* state) {
    auto* references = static_cast<
        std::vector<ClassRuntimeState::ClassRecord::ConfigReference>*>(
        luaL_checkudata(state, 1, configReferenceMetatable));
    references->~vector();
    return 0;
}

std::optional<std::vector<ClassRuntimeState::ClassRecord::ConfigReference>>
cachedConfigReferences(const RuntimeValue& cached) {
    if (cached.isNil()) {
        return std::nullopt;
    }
    RuntimeScope scope;
    lua_State* state = scope.state();
    const lua_glue::Object value =
        binding::writeLuaValue(lua_glue::StateView(state), cached);
    value.push(state);
    const auto* references = static_cast<
        const std::vector<ClassRuntimeState::ClassRecord::ConfigReference>*>(
        luaL_testudata(state, -1, configReferenceMetatable));
    lua_pop(state, 1);
    return references == nullptr ? std::nullopt : std::optional(*references);
}

RuntimeValue storeConfigReferences(
    const std::vector<ClassRuntimeState::ClassRecord::ConfigReference>&
        references) {
    RuntimeScope scope;
    lua_State* state = scope.state();
    if (luaL_newmetatable(state, configReferenceMetatable)) {
        lua_pushcfunction(state, destroyConfigReferences);
        lua_setfield(state, -2, "__gc");
    }
    lua_pop(state, 1);
    new (lua_newuserdatauv(
        state,
        sizeof(std::vector<ClassRuntimeState::ClassRecord::ConfigReference>),
        0))
        std::vector<ClassRuntimeState::ClassRecord::ConfigReference>(
            references);
    luaL_setmetatable(state, configReferenceMetatable);
    const lua_glue::Object stored = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return detail::readRuntimeReference(stored);
}

}  // namespace

std::shared_ptr<Graph> compileGraphTemplate(const RuntimeValue& data,
                                            const RuntimeValue& classType) {
    const RuntimeValue graphData = rawGet(intern(data), "graph");
    return isTable(graphData) ? runtimeProviders().compileBlueprintGraph(
                                    identity(graphData), identity(classType))
                              : nullptr;
}

bool classGraphHasExecutableEvent(const std::string& classPath,
                                  const std::string& eventName) {
    const auto& records = resolverState().records;
    const auto record = records.find(classPath);
    return record != records.end() &&
           record->second->graphTemplate != nullptr &&
           record->second->graphTemplate->hasExecutableEvent(eventName);
}

std::shared_ptr<Graph> instantiateClassGraph(const std::string& classPath,
                                             const RuntimeValue& parent) {
    auto& records = resolverState().records;
    auto found = records.find(classPath);
    if (found == records.end()) {
        resolveClass(RuntimeValue(classPath), RuntimeValue());
        found = records.find(classPath);
    }
    if (found == records.end() || found->second->scriptMixin ||
        found->second->graphTemplate == nullptr) {
        return nullptr;
    }
    return runtimeProviders().instantiateBlueprintGraph(
        found->second->graphTemplate, identity(parent));
}

std::string declaringModule(const RuntimeValue& value) {
    return is<std::string>(value) ? as<std::string>(value) : std::string();
}

RuntimeValue cloneMetadataValue(const RuntimeValue& value,
                                const RuntimeValue& fieldMetadata,
                                const std::string& fallbackModule,
                                bool stored) {
    TypedDataService& dataValues = typedDataService();
    const RuntimeValue typeReference =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "type");
    const RuntimeValue module =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "module");
    const RuntimeValue runtimeType =
        typeReference.getIf<RuntimeHandle>() != nullptr
            ? snapshot(typeReference)
            : typeReference;
    const std::string fieldModule = declaringModule(module);
    const std::string moduleName =
        fieldModule.empty() ? fallbackModule : fieldModule;

    const RuntimeValue component =
        rawGet(ludork::runtime::reference::intern(fieldMetadata), "component");
    if (is<bool>(component) && as<bool>(component)) {
        const RuntimeValue componentType =
            dataValues.resolveMetadataType(runtimeType, moduleName);
        if (!componentType.isNil()) {
            return ludork::runtime::components::componentFromData(componentType,
                                                                  value);
        }
        return deepCopy(value);
    }
    if (!stored) {
        return deepCopy(dataValues.resolveRuntimeTypedValue(value, runtimeType,
                                                            moduleName));
    }
    return deepCopy(dataValues.resolveTypedDataValue(
        value, runtimeType, RuntimeValue::Map{}, moduleName));
}

RuntimeValue cloneAttrValue(const RuntimeValue& parentClass,
                            const RuntimeValue& key, const RuntimeValue& value,
                            const RuntimeValue& rawMetadata,
                            const RuntimeValue& rawTargetType, bool stored) {
    TypedDataService& dataValues = typedDataService();
    if (isTable(rawMetadata)) {
        return cloneMetadataValue(value, rawMetadata, {}, stored);
    }
    RuntimeValue targetType;
    if (!rawTargetType.isNil()) {
        targetType = rawTargetType.getIf<RuntimeHandle>() != nullptr
                         ? snapshot(rawTargetType)
                         : rawTargetType;
    } else if (is<std::string>(key)) {
        targetType =
            dataValues.resolveAttrValueType(parentClass, as<std::string>(key));
    }
    const std::string* targetName = targetType.getIf<std::string>();
    if (!targetType.isNil() &&
        (targetName == nullptr || *targetName != "any")) {
        return deepCopy(
            stored ? dataValues.resolveTypedDataValue(value, targetType)
                   : dataValues.resolveRuntimeTypedValue(value, targetType));
    }
    return deepCopy(value);
}

ClassRuntimeState::ClassRecord::InstanceAttributePlan compileAttributePlan(
    const std::string& name, const RuntimeValue& value,
    const RuntimeValue& parentClass, const RuntimeValue& fieldMetadata,
    const RuntimeValue& targetType, bool copyOnly) {
    ClassRuntimeState::ClassRecord::InstanceAttributePlan plan{
        name, value, std::nullopt, {}, RuntimeValue()};
    if (copyOnly) {
        return plan;
    }
    RuntimeValue type;
    if (isTable(fieldMetadata)) {
        type = snapshot(rawGet(intern(fieldMetadata), "type"));
        plan.declaringModule =
            declaringModule(rawGet(intern(fieldMetadata), "module"));
        if (boolean(rawGet(intern(fieldMetadata), "component"))) {
            plan.componentType = typedDataService().resolveMetadataType(
                type, plan.declaringModule);
            return plan;
        }
    } else {
        type = targetType.isNil()
                   ? typedDataService().resolveAttrValueType(parentClass, name)
                   : snapshot(targetType);
    }
    const std::string* typeName = type.getIf<std::string>();
    if (!type.isNil() && (typeName == nullptr || *typeName != "any")) {
        plan.schema = typedDataService().compileType(type.view());
    }
    return plan;
}

std::vector<ClassRuntimeState::ClassRecord::ConfigReference> configReferences(
    const RuntimeValue& owner) {
    const RuntimeHandle cache = resolverState().configReferenceCache;
    if (auto cached = cachedConfigReferences(rawGet(cache, owner))) {
        return std::move(*cached);
    }

    std::unordered_map<std::string,
                       ClassRuntimeState::ClassRecord::ConfigReference>
        references;
    std::vector<RuntimeValue> mro = classMro(intern(owner));
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        for (const auto& [name, reference] :
             detail::classConfigReferences(*current)) {
            const RuntimeValue config = rawGet(intern(reference), 1);
            const RuntimeValue setting = rawGet(intern(reference), 2);
            if (is<std::string>(config) && is<std::string>(setting)) {
                references.insert_or_assign(
                    name, ClassRuntimeState::ClassRecord::ConfigReference{
                              name, as<std::string>(config),
                              as<std::string>(setting)});
            }
        }
    }
    std::vector<ClassRuntimeState::ClassRecord::ConfigReference> result;
    result.reserve(references.size());
    for (auto& [name, reference] : references) {
        result.push_back(std::move(reference));
    }
    rawSet(cache, owner, storeConfigReferences(result));
    return result;
}

RuntimeValue resolveConfigValue(
    const RuntimeValue& value,
    const ClassRuntimeState::ClassRecord::ConfigReference& reference) {
    if (!is<std::string>(value) || !as<std::string>(value).empty()) {
        return value;
    }
    return RuntimeValue(
        runtimeProviders().config(reference.config, reference.setting));
}

void applyConfigValues(
    const RuntimeValue& parentClass, RuntimeValue classAttrs,
    const std::vector<ClassRuntimeState::ClassRecord::ConfigReference>&
        references) {
    for (const ClassRuntimeState::ClassRecord::ConfigReference& reference :
         references) {
        const RuntimeValue current = rawGet(intern(classAttrs), reference.name);
        if (!current.isNil()) {
            rawSet(intern(classAttrs), reference.name,
                   resolveConfigValue(current, reference));
            continue;
        }
        RuntimeValue parentValue = get(intern(parentClass), reference.name);
        if (parentValue.isNil()) {
            parentValue = RuntimeValue(std::string());
        }
        const RuntimeValue resolved =
            resolveConfigValue(parentValue, reference);
        if (!rawEqual(parentValue, resolved)) {
            rawSet(intern(classAttrs), reference.name, resolved);
        }
    }
}

void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const RuntimeValue& self,
                                 const RuntimeValue::Array& arguments) {
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    const auto& records = resolverState().records;
    const auto found = records.find(classPath);
    if (found == records.end()) {
        return;
    }
    const std::shared_ptr<const ClassRuntimeState::ClassRecord> record =
        found->second;
    std::unordered_set<std::string> appliedAttrs;
    const RuntimeHandle instance = intern(self);
    for (auto current = record; current != nullptr;
         current = current->parentRecord) {
        for (const ClassRuntimeState::ClassRecord::InstanceAttributePlan& plan :
             current->attributes) {
            if (!appliedAttrs.insert(plan.name).second ||
                hasOwnField(instance, RuntimeValue(plan.name))) {
                continue;
            }
            RuntimeValue value;
            if (!plan.componentType.isNil()) {
                value = components::componentFromData(plan.componentType,
                                                      plan.defaultValue);
            } else if (plan.schema.has_value()) {
                value = deepCopy(typedDataService().resolveRuntimeTypedValue(
                    plan.defaultValue, *plan.schema, plan.declaringModule));
            } else {
                value = deepCopy(plan.defaultValue);
            }
            runtimeReflection().setTyped(instance, plan.name, value);
        }
    }
    if (isFunction(record->parentInit)) {
        RuntimeValue::Array values;
        values.reserve(arguments.size() + 1);
        values.push_back(self);
        values.insert(values.end(), arguments.begin(), arguments.end());
        static_cast<void>(invoke(record->parentInit, values));
    }
}

}  // namespace ludork::runtime::class_runtime_detail
