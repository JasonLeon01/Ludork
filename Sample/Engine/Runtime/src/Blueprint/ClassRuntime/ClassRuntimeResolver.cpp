#include <Runtime/RuntimeReference.hpp>
#include "ClassRuntimeInternal.hpp"
#include "RuntimeServiceInternals.hpp"
#include "RuntimeBindingTraits.hpp"
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <Runtime/Components/ComponentRuntime.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <Runtime/RuntimeProviders.hpp>
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

RuntimeHandle resolverState() {
    RuntimeHandle registry = ludork::runtime::reference::registry();
    const RuntimeValue existing = rawGet(registry, CLASS_RESOLVER_STATE_KEY);
    if (isTable(existing)) {
        return intern(existing);
    }
    RuntimeHandle state = table();
    RuntimeHandle classes = table();
    rawSet(state, "classes", classes);
    rawSet(state, "classData", table());
    rawSet(state, "records", table());
    rawSet(state, "classNames", table());
    RuntimeHandle configReferenceCache = table();
    RuntimeHandle configReferenceCacheMetatable = table();
    rawSet(configReferenceCacheMetatable, "__mode", "k");
    setMetatable(configReferenceCache, configReferenceCacheMetatable);
    rawSet(state, "configReferences", configReferenceCache);
    rawSet(registry, CLASS_RESOLVER_STATE_KEY, state);
    return state;
}

namespace {

void indexClassPath(const RuntimeHandle& state, const std::string& classPath,
                    bool dataBacked) {
    std::string className;
    if (dataBacked) {
        className = classPath;
        std::replace(className.begin(), className.end(), '.', '_');
    } else {
        className = classPath.substr(classPath.find_last_of('.') + 1);
    }
    rawSet(requireTable(rawGet(state, "classNames")), className, classPath);
}

}  // namespace

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
        rawGet(ludork::runtime::reference::intern(module), "__ludorkClass");
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
    RuntimeHandle state = resolverState();
    RuntimeHandle classes = requireTable(rawGet(state, "classes"));
    RuntimeHandle classData = requireTable(rawGet(state, "classData"));
    const RuntimeValue cached = rawGet(classes, classPath);
    if (!cached.isNil()) {
        return {cached, rawGet(classData, classPath)};
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
            rawSet(classes, classPath, targetClass);
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
    rawSet(classData, classPath, definitionData);
    const RuntimeValue rawParentPath =
        rawGet(ludork::runtime::reference::intern(definitionData), "parent");
    if (!is<std::string>(rawParentPath)) {
        throw std::runtime_error("Class parent is missing: " + classPath);
    }
    const std::string parentPath = as<std::string>(rawParentPath);
    RuntimeValue parentClass = rawGet(classes, parentPath);
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

    applyConfigValues(parentClass, classAttrs, configReferences(parentClass));
    RuntimeScope scope;
    sol::state_view lua(scope.state());
    const sol::object metadataOwner = binding::writeLuaValue(lua, parentClass);
    const RuntimeValue rawMetadata = detail::readRuntimeReference(
        sol::make_object(lua, detail::collectRuntimeAttrMetadata(
                                  lua, metadataOwner.as<sol::table>())));
    const RuntimeValue attrMetadata =
        isTable(rawMetadata) ? rawMetadata : table();
    RuntimeHandle attrTypes = table();

    RuntimeHandle definition = table();
    RuntimeHandle instanceAttrs = table();
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
        rawSet(definition, entry.first,
               cloneAttrValue(parentClass, entry.first, entry.second,
                              rawFieldMetadata, targetType));
        rawSet(instanceAttrs, entry.first, deepCopy(entry.second));
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

    RuntimeHandle record = table();
    const RuntimeValue parentRecord =
        rawGet(requireTable(rawGet(state, "records")), parentPath);
    rawSet(record, "attrs", instanceAttrs);
    rawSet(record, "parent", parentClass);
    rawSet(record, "parentRecord", parentRecord);
    rawSet(record, "metadata", attrMetadata);
    rawSet(record, "types", attrTypes);
    rawSet(record, "scriptMixin", scriptMixin);
    if (isTable(rawMixin)) {
        rawSet(record, "scriptTable", rawMixin);
    }
    if (!normalizedScriptPath.empty()) {
        rawSet(record, "scriptPath", normalizedScriptPath);
    }
    rawSet(record, "parentInit",
           isTable(parentRecord)
               ? rawGet(ludork::runtime::reference::intern(parentRecord),
                        "parentInit")
               : get(ludork::runtime::reference::intern(parentClass), "init"));
    if (!scriptMixin) {
        const RuntimeValue graphTemplate =
            compileGraphTemplate(definitionData, targetClass);
        if (!graphTemplate.isNil()) {
            rawSet(record, "graphTemplate", graphTemplate);
        }
    }
    rawSet(requireTable(rawGet(state, "records")), classPath, record);
    rawSet(classes, classPath, generatedClass);
    indexClassPath(state, classPath, true);
    return {generatedClass, definitionData};
}

}  // namespace ludork::runtime::class_runtime_detail
