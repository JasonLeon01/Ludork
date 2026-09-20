#include <Runtime/RuntimeProviderFacade.hpp>
#include <ClassRuntimeProtocol.hpp>
#include <Runtime/RuntimeReference.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include "ClassRuntimeInternal.hpp"
#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeReferenceConversion.hpp"
#include "LuaServices/RuntimeServiceInternals.hpp"
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <Runtime/TypedDataService.hpp>
#include <Runtime/Json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <new>

extern "C" {
#include <lauxlib.h>
}

namespace ludork::runtime::class_runtime_detail {

using namespace ludork::runtime::reference;

RuntimeHandle requireModuleTable(const std::string& moduleName) {
    const RuntimeValue module = requireModule(moduleName);
    if (!isTable(module)) {
        throw std::runtime_error("Lua module did not return a table: " +
                                 moduleName);
    }
    return intern(module);
}

namespace {

constexpr const char* resolverMetatable = "Ludork.Runtime.ClassRuntimeState";

int destroyResolver(lua_State* state) {
    auto* storage = static_cast<std::unique_ptr<ClassRuntimeState>*>(
        luaL_checkudata(state, 1, resolverMetatable));
    storage->~unique_ptr();
    return 0;
}

void indexClassPath(ClassRuntimeState& state, const std::string& classPath,
                    bool dataBacked) {
    std::string className;
    if (dataBacked) {
        className = classPath;
        std::replace(className.begin(), className.end(), '.', '_');
    } else {
        className = classPath.substr(classPath.find_last_of('.') + 1);
    }
    state.classNames.insert_or_assign(std::move(className), classPath);
}

}  // namespace

ClassRuntimeState* existingResolverState(lua_State* state) noexcept {
    lua_getfield(state, LUA_REGISTRYINDEX, CLASS_RESOLVER_STATE_KEY);
    auto* storage = static_cast<std::unique_ptr<ClassRuntimeState>*>(
        luaL_testudata(state, -1, resolverMetatable));
    ClassRuntimeState* result = storage == nullptr ? nullptr : storage->get();
    lua_pop(state, 1);
    return result;
}

ClassRuntimeState& resolverState() {
    RuntimeScope scope;
    lua_State* state = scope.state();
    if (ClassRuntimeState* existing = existingResolverState(state)) {
        return *existing;
    }
    const RuntimeHandle configReferenceCache = table(WeakMode::Keys);
    if (luaL_newmetatable(state, resolverMetatable)) {
        lua_pushcfunction(state, destroyResolver);
        lua_setfield(state, -2, "__gc");
    }
    lua_pop(state, 1);
    auto* storage = new (
        lua_newuserdatauv(state, sizeof(std::unique_ptr<ClassRuntimeState>), 0))
        std::unique_ptr<ClassRuntimeState>(
            std::make_unique<ClassRuntimeState>());
    (*storage)->configReferenceCache = configReferenceCache;
    luaL_setmetatable(state, resolverMetatable);
    lua_setfield(state, LUA_REGISTRYINDEX, CLASS_RESOLVER_STATE_KEY);
    return **storage;
}

void clearResolverState(lua_State* state) noexcept {
    lua_getfield(state, LUA_REGISTRYINDEX, CLASS_RESOLVER_STATE_KEY);
    auto* storage = static_cast<std::unique_ptr<ClassRuntimeState>*>(
        luaL_testudata(state, -1, resolverMetatable));
    if (storage != nullptr) {
        storage->reset();
    }
    lua_pop(state, 1);
    lua_pushnil(state);
    lua_setfield(state, LUA_REGISTRYINDEX, CLASS_RESOLVER_STATE_KEY);
}

std::optional<std::string> directModuleMetadataType(
    const std::string& moduleName) {
    const std::string metadataModule = moduleName + "_meta";
    if (!moduleExists(metadataModule)) {
        return std::nullopt;
    }
    const RuntimeHandle metadata = requireModuleTable(metadataModule);
    std::optional<std::string> result;
    for (const auto& entry : entries(metadata)) {
        if (!is<std::string>(entry.first) || !isTable(entry.second)) {
            continue;
        }
        if (result.has_value()) {
            throw std::runtime_error(
                "Metadata module for directly returned class must contain one "
                "type: " +
                metadataModule);
        }
        result = as<std::string>(entry.first);
    }
    if (!result.has_value()) {
        throw std::runtime_error(
            "Metadata module for directly returned class contains no type: " +
            metadataModule);
    }
    return result;
}

RuntimeValue moduleClass(const RuntimeValue& rawModule,
                         const std::string& moduleName,
                         const std::string& className) {
    if (!isTable(rawModule)) {
        return RuntimeValue();
    }
    const RuntimeValue module = rawModule;
    const RuntimeValue classMarker =
        rawGet(ludork::runtime::reference::intern(module),
               ludork::standard::class_runtime::protocol::CLASS_MARKER_FIELD);
    if (is<bool>(classMarker) && as<bool>(classMarker)) {
        const std::size_t separator = moduleName.find_last_of('.');
        const std::string moduleType = separator == std::string::npos
                                           ? moduleName
                                           : moduleName.substr(separator + 1);
        if (moduleType == className) {
            return rawModule;
        }
        const std::optional<std::string> metadataType =
            directModuleMetadataType(moduleName);
        return metadataType.has_value() && *metadataType == className
                   ? rawModule
                   : RuntimeValue();
    }
    const RuntimeValue member =
        rawGet(ludork::runtime::reference::intern(module), className);
    return !member.isNil() ? member : RuntimeValue();
}

std::tuple<RuntimeValue, RuntimeValue> resolveClass(
    const RuntimeValue& rawPath, const RuntimeValue& rawRoot) {
    if (!is<std::string>(rawPath)) {
        return {RuntimeValue(), RuntimeValue()};
    }
    const std::string classPath = as<std::string>(rawPath);
    ClassRuntimeState& state = resolverState();
    const auto cached = state.records.find(classPath);
    if (cached != state.records.end()) {
        return {RuntimeValue(cached->second->classType),
                cached->second->definition};
    }

    const std::size_t separator = classPath.find_last_of('.');
    if (separator == std::string::npos) {
        throw std::runtime_error("Class " + classPath + " not found");
    }
    const std::string modulePath = classPath.substr(0, separator);
    const std::string className = classPath.substr(separator + 1);
    RuntimeValue rawData = RuntimeValue();
    if (!is<std::string>(rawRoot) || as<std::string>(rawRoot).empty()) {
        rawData =
            RuntimeValue(runtimeProviders().blueprintClassData(classPath));
    }
    RuntimeValue targetClass = RuntimeValue();
    if (!isTable(rawData)) {
        if (moduleExists(classPath)) {
            targetClass = moduleClass(requireModuleTable(classPath), classPath,
                                      className);
        }
        if (targetClass.isNil() && moduleExists(modulePath)) {
            targetClass = moduleClass(requireModuleTable(modulePath),
                                      modulePath, className);
        }
        if (!targetClass.isNil()) {
            auto record = std::make_shared<ClassRuntimeState::ClassRecord>();
            record->classType = intern(targetClass);
            state.records.emplace(classPath, std::move(record));
            indexClassPath(state, classPath, false);
            return {targetClass, RuntimeValue()};
        }

        std::string filePath = classPath;
        std::replace(filePath.begin(), filePath.end(), '.', '/');
        if (is<std::string>(rawRoot) && !as<std::string>(rawRoot).empty()) {
            filePath = as<std::string>(rawRoot) + "/" + filePath;
        }
        filePath += ".json";
        if (!jsonExists(filePath)) {
            throw std::runtime_error("Class " + classPath + " not found");
        }
        rawData = intern(getJSONData(filePath));
    }
    if (!isTable(rawData)) {
        throw std::runtime_error("Class data must be a table: " + classPath);
    }
    RuntimeValue definitionData = rawData;
    const RuntimeValue rawParentPath =
        rawGet(ludork::runtime::reference::intern(definitionData), "parent");
    if (!is<std::string>(rawParentPath)) {
        throw std::runtime_error("Class parent is missing: " + classPath);
    }
    const std::string parentPath = as<std::string>(rawParentPath);
    const auto parentEntry = state.records.find(parentPath);
    RuntimeValue parentClass =
        parentEntry != state.records.end()
            ? RuntimeValue(parentEntry->second->classType)
            : RuntimeValue();
    if (!isTable(parentClass)) {
        parentClass = std::get<0>(resolveClass(rawParentPath, rawRoot));
    }
    if (!isTable(parentClass)) {
        throw std::runtime_error("Class parent was not resolved: " +
                                 parentPath);
    }

    const RuntimeValue rawAttrs =
        rawGet(ludork::runtime::reference::intern(definitionData), "attrs");
    RuntimeValue copiedAttrs =
        isTable(rawAttrs) ? deepCopy(rawAttrs) : RuntimeValue(table());
    RuntimeValue classAttrs = copiedAttrs;
    rawSet(ludork::runtime::reference::intern(definitionData), "attrs",
           classAttrs);
    const RuntimeValue parentClassTable = parentClass;
    const RuntimeValue rawParentScriptMixin = get(
        ludork::runtime::reference::intern(parentClassTable), "scriptMixin");
    const bool parentScriptMixin =
        is<bool>(rawParentScriptMixin) && as<bool>(rawParentScriptMixin);
    const RuntimeValue rawLocalScriptMixin =
        rawGet(ludork::runtime::reference::intern(classAttrs), "scriptMixin");
    const bool hasLocalScriptMixin = !rawLocalScriptMixin.isNil();
    if (hasLocalScriptMixin && !is<bool>(rawLocalScriptMixin)) {
        throw std::runtime_error("scriptMixin must be a boolean: " + classPath);
    }
    const bool scriptMixin =
        hasLocalScriptMixin ? as<bool>(rawLocalScriptMixin) : parentScriptMixin;
    const bool parentIsBlueprint = parentPath.starts_with("Data.Blueprints.");
    if (parentIsBlueprint && hasLocalScriptMixin &&
        scriptMixin != parentScriptMixin) {
        throw std::runtime_error(
            "Blueprint inheritance cannot mix ScriptMixin and graph modes: " +
            classPath + " -> " + parentPath);
    }
    const RuntimeValue rawLocalScriptPath =
        rawGet(ludork::runtime::reference::intern(classAttrs), "scriptPath");
    const bool hasLocalScriptPath = !rawLocalScriptPath.isNil();
    if (hasLocalScriptPath && !is<std::string>(rawLocalScriptPath)) {
        throw std::runtime_error("scriptPath must be a string: " + classPath);
    }
    const std::string localScriptPath =
        hasLocalScriptPath ? as<std::string>(rawLocalScriptPath) : "";
    if (scriptMixin && !parentIsBlueprint && localScriptPath.empty()) {
        throw std::runtime_error(
            "Root ScriptMixin blueprint must declare scriptPath: " + classPath);
    }
    if (scriptMixin && hasLocalScriptPath && localScriptPath.empty()) {
        throw std::runtime_error("Local scriptPath must be non-empty: " +
                                 classPath);
    }

    const std::vector<ClassRuntimeState::ClassRecord::ConfigReference>
        references = configReferences(parentClass);
    applyConfigValues(parentClass, classAttrs, references);
    RuntimeScope scope;
    lua_glue::StateView lua(scope.state());
    const lua_glue::Object metadataOwner =
        binding::writeLuaValue(lua, parentClass);
    const RuntimeValue rawMetadata =
        detail::readRuntimeReference(lua_glue::MakeObject(
            lua, detail::collectRuntimeAttrMetadata(
                     lua, metadataOwner.as<lua_glue::Table>())));
    const RuntimeValue attrMetadata =
        isTable(rawMetadata) ? rawMetadata : table();
    RuntimeHandle attrTypes = table();

    RuntimeHandle definition = table();
    RuntimeHandle instanceAttrs = table();
    RuntimeHandle copyAttrs = table();
    RuntimeHandle nilAttrs = table();
    RuntimeValue rawMixin = RuntimeValue();
    std::string normalizedScriptPath;
    if (scriptMixin && !localScriptPath.empty()) {
        normalizedScriptPath = normalizeScriptMixinPath(localScriptPath);
        RuntimeValue mixin = loadScriptMixin(classPath, normalizedScriptPath);
        mergeScriptMixin(parentClassTable, mixin, definition, instanceAttrs,
                         classPath, normalizedScriptPath);
        rawMixin = mixin;
    }
    for (const auto& entry :
         entries(ludork::runtime::reference::intern(classAttrs))) {
        const RuntimeValue mixinMember = rawGet(definition, entry.first);
        if (kind(mixinMember) == "function") {
            throw std::runtime_error(
                "Blueprint attr cannot replace Mixin method '" +
                as<std::string>(entry.first) + "': " + classPath);
        }
        const RuntimeValue rawFieldMetadata = rawGet(
            ludork::runtime::reference::intern(attrMetadata), entry.first);
        RuntimeValue targetType = RuntimeValue();
        if (!isTable(rawFieldMetadata)) {
            if (is<std::string>(entry.first)) {
                targetType = detail::readRuntimeReference(
                    detail::resolveRuntimeAttrValueType(
                        lua, metadataOwner, as<std::string>(entry.first)));
            }
            if (!targetType.isNil()) {
                rawSet(attrTypes, entry.first, targetType);
            }
        }
        const RuntimeValue resolvedValue =
            cloneAttrValue(parentClass, entry.first, entry.second,
                           rawFieldMetadata, targetType);
        if (resolvedValue.isNil() && is<std::string>(entry.first)) {
            const std::string name = as<std::string>(entry.first);
            runtimeReflection().setTyped(definition, name, resolvedValue);
            rawSet(nilAttrs, name, true);
        } else {
            rawSet(definition, entry.first, resolvedValue);
        }
        rawSet(instanceAttrs, entry.first, deepCopy(resolvedValue));
        const RuntimeValue component =
            isTable(rawFieldMetadata)
                ? rawGet(ludork::runtime::reference::intern(rawFieldMetadata),
                         "component")
                : RuntimeValue();
        if (!boolean(component)) {
            rawSet(copyAttrs, entry.first, true);
        }
    }

    rawSet(definition, "_GENERATED_CLASS", true);
    rawSet(definition, "__blueprintClassPath", classPath);
    rawSet(
        definition, "init",
        callback(
            [state = ludork::runtime::RuntimeScope().state(), classPath](
                const RuntimeValue::Array& arguments) -> RuntimeValue::Array {
                if (arguments.empty()) {
                    throw std::invalid_argument(
                        "Generated initializer requires self");
                }
                RuntimeValue::Array parameters(arguments.begin() + 1,
                                               arguments.end());
                initializeGeneratedInstance(state, classPath, arguments.front(),
                                            parameters);
                return {};
            }));
    RuntimeHandle bases = table();
    rawSet(bases, 1, parentClass);
    RuntimeValue generatedClass = finalizeClass(definition, bases);
    targetClass = generatedClass;

    auto record = std::make_shared<ClassRuntimeState::ClassRecord>();
    record->classType = intern(generatedClass);
    record->definition = definitionData;
    record->parentClass = intern(parentClass);
    record->parentRecord = state.records.at(parentPath);
    record->configReferences = references;
    for (const auto& [key, value] : entries(instanceAttrs)) {
        if (is<std::string>(key)) {
            record->attributes.push_back(compileAttributePlan(
                as<std::string>(key), value, parentClass,
                rawGet(intern(attrMetadata), key), rawGet(attrTypes, key),
                boolean(rawGet(copyAttrs, key))));
        }
    }
    for (const auto& [key, value] : entries(nilAttrs)) {
        if (is<std::string>(key)) {
            record->attributes.push_back({as<std::string>(key),
                                          RuntimeValue(),
                                          std::nullopt,
                                          {},
                                          RuntimeValue()});
        }
    }
    record->scriptMixin = scriptMixin;
    record->scriptTable = rawMixin;
    record->scriptPath = normalizedScriptPath;
    record->parentInit =
        !RuntimeValue(record->parentRecord->parentClass).isNil()
            ? record->parentRecord->parentInit
            : intern(get(intern(parentClass), "init"));
    if (!scriptMixin) {
        record->graphTemplate =
            compileGraphTemplate(definitionData, targetClass);
    }
    state.records.emplace(classPath, std::move(record));
    indexClassPath(state, classPath, true);
    return {generatedClass, definitionData};
}

}  // namespace ludork::runtime::class_runtime_detail
