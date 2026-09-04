#include "EngineClassRuntimeInternal.hpp"

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <Runtime/RuntimeProviders.hpp>
#include <RuntimeSession.hpp>
#include <Utils/DataValue.hpp>
#include <Utils/File.hpp>

#include <sol2/sol.hpp>

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

namespace ludork::engine::class_runtime_detail {

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result, int index) {
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    if (result.return_count() <= index) {
        return nilObject(lua);
    }
    return result.get<sol::object>(index);
}

sol::table requireTable(sol::state_view lua, const std::string& moduleName) {
    const sol::object module =
        ludork::standard::class_runtime::requireModule(lua, moduleName);
    if (!module.is<sol::table>()) {
        throw std::runtime_error("Lua module did not return a table: " +
                                 moduleName);
    }
    return module.as<sol::table>();
}

sol::table resolverState(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object existing =
        registry.raw_get<sol::object>(CLASS_RESOLVER_STATE_KEY);
    if (existing.is<sol::table>()) {
        return existing.as<sol::table>();
    }
    sol::table state = lua.create_table();
    sol::table classes = lua.create_table();
    classes.raw_set("", lua.create_table());
    state.raw_set("classes", classes);
    state.raw_set("classData", lua.create_table());
    state.raw_set("records", lua.create_table());
    sol::table configReferenceCache = lua.create_table();
    sol::table configReferenceCacheMetatable = lua.create_table();
    configReferenceCacheMetatable.raw_set("__mode", "k");
    configReferenceCache[sol::metatable_key] = configReferenceCacheMetatable;
    state.raw_set("configReferences", configReferenceCache);
    registry.raw_set(CLASS_RESOLVER_STATE_KEY, state);
    return state;
}

std::tuple<sol::object, sol::object> resolveClass(sol::state_view lua,
                                                  const sol::object& rawPath,
                                                  const sol::object& rawRoot);

bool moduleExists(sol::state_view lua, const std::string& moduleName) {
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return false;
    }
    const sol::table package = rawPackage.as<sol::table>();
    for (const char* field : {"loaded", "preload"}) {
        const sol::object rawModules = package.raw_get<sol::object>(field);
        if (!rawModules.is<sol::table>()) {
            continue;
        }
        const sol::object module =
            rawModules.as<sol::table>().raw_get<sol::object>(moduleName);
        if (module.valid() && module.get_type() != sol::type::lua_nil) {
            return true;
        }
    }
    const sol::object rawSearch = package.raw_get<sol::object>("searchpath");
    if (!rawSearch.is<sol::protected_function>()) {
        return false;
    }
    sol::protected_function search = rawSearch.as<sol::protected_function>();
    for (const char* field : {"path", "cpath"}) {
        const sol::object rawPath = package.raw_get<sol::object>(field);
        if (!rawPath.is<std::string>()) {
            continue;
        }
        sol::protected_function_result result = search(moduleName, rawPath);
        if (result.valid() && result.return_count() > 0) {
            const sol::object found = result.get<sol::object>();
            if (found.is<std::string>()) {
                return true;
            }
        }
    }
    return false;
}

std::optional<std::string> directModuleMetadataType(
    sol::state_view lua, const std::string& moduleName) {
    const std::string metadataModule = moduleName + "_meta";
    if (!moduleExists(lua, metadataModule)) {
        return std::nullopt;
    }
    const sol::table metadata = requireTable(lua, metadataModule);
    std::optional<std::string> result;
    for (const auto& entry : metadata) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        if (result.has_value()) {
            throw std::runtime_error(
                "Metadata module for directly returned class must contain one "
                "type: " +
                metadataModule);
        }
        result = entry.first.as<std::string>();
    }
    if (!result.has_value()) {
        throw std::runtime_error(
            "Metadata module for directly returned class contains no type: " +
            metadataModule);
    }
    return result;
}

sol::object moduleClass(sol::state_view lua, const sol::object& rawModule,
                        const std::string& moduleName,
                        const std::string& className) {
    if (!rawModule.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table module = rawModule.as<sol::table>();
    const sol::object classMarker =
        module.raw_get<sol::object>("__ludorkClass");
    if (classMarker.is<bool>() && classMarker.as<bool>()) {
        const std::size_t separator = moduleName.find_last_of('.');
        const std::string moduleType = separator == std::string::npos
                                           ? moduleName
                                           : moduleName.substr(separator + 1);
        if (moduleType == className) {
            return rawModule;
        }
        const std::optional<std::string> metadataType =
            directModuleMetadataType(lua, moduleName);
        return metadataType.has_value() && *metadataType == className
                   ? rawModule
                   : nilObject(lua);
    }
    const sol::object member = module.raw_get<sol::object>(className);
    return member.valid() ? member : nilObject(lua);
}

std::tuple<sol::object, sol::object> resolveClass(sol::state_view lua,
                                                  const sol::object& rawPath,
                                                  const sol::object& rawRoot) {
    if (!rawPath.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string classPath = rawPath.as<std::string>();
    sol::table state = resolverState(lua);
    sol::table classes = state.raw_get<sol::table>("classes");
    sol::table classData = state.raw_get<sol::table>("classData");
    const sol::object cached = classes.raw_get<sol::object>(classPath);
    if (cached.valid() && cached.get_type() != sol::type::lua_nil) {
        return {cached, classData.raw_get<sol::object>(classPath)};
    }

    const std::size_t separator = classPath.find_last_of('.');
    if (separator == std::string::npos) {
        throw std::runtime_error("Class " + classPath + " not found");
    }
    const std::string modulePath = classPath.substr(0, separator);
    const std::string className = classPath.substr(separator + 1);
    sol::object rawData = nilObject(lua);
    if (!rawRoot.is<std::string>() || rawRoot.as<std::string>().empty()) {
        rawData = runtimeIdentityValue(
            lua, runtimeProviders().blueprintClassData(classPath));
    }
    sol::object targetClass = nilObject(lua);
    if (!rawData.is<sol::table>()) {
        if (moduleExists(lua, classPath)) {
            targetClass = moduleClass(lua, requireTable(lua, classPath),
                                      classPath, className);
        }
        if (targetClass.get_type() == sol::type::lua_nil &&
            moduleExists(lua, modulePath)) {
            targetClass = moduleClass(lua, requireTable(lua, modulePath),
                                      modulePath, className);
        }
        if (targetClass.valid() &&
            targetClass.get_type() != sol::type::lua_nil) {
            classes.raw_set(classPath, targetClass);
            return {targetClass, nilObject(lua)};
        }

        std::string filePath = classPath;
        std::replace(filePath.begin(), filePath.end(), '.', '/');
        if (rawRoot.is<std::string>() && !rawRoot.as<std::string>().empty()) {
            filePath = rawRoot.as<std::string>() + "/" + filePath;
        }
        filePath += ".json";
        if (!jsonExists(filePath)) {
            throw std::runtime_error("Class " + classPath + " not found");
        }
        rawData = luaValue(lua, getJSONData(filePath));
    }
    if (!rawData.is<sol::table>()) {
        throw std::runtime_error("Class data must be a table: " + classPath);
    }
    sol::table data = rawData.as<sol::table>();
    classData.raw_set(classPath, data);
    const sol::object rawParentPath = data.raw_get<sol::object>("parent");
    if (!rawParentPath.is<std::string>()) {
        throw std::runtime_error("Class parent is missing: " + classPath);
    }
    const std::string parentPath = rawParentPath.as<std::string>();
    sol::object parentClass = classes.raw_get<sol::object>(parentPath);
    if (!parentClass.is<sol::table>()) {
        parentClass = std::get<0>(resolveClass(lua, rawParentPath, rawRoot));
    }
    if (!parentClass.is<sol::table>()) {
        throw std::runtime_error("Class parent was not resolved: " +
                                 parentPath);
    }

    const sol::object rawAttrs = data.raw_get<sol::object>("attrs");
    sol::object copiedAttrs =
        rawAttrs.is<sol::table>()
            ? ludork::standard::class_runtime::deepCopy(lua, rawAttrs)
            : sol::make_object(lua, lua.create_table());
    sol::table classAttrs = copiedAttrs.as<sol::table>();
    data.raw_set("attrs", classAttrs);
    const sol::table parentClassTable = parentClass.as<sol::table>();
    const sol::object rawParentScriptMixin =
        parentClassTable.get<sol::object>("scriptMixin");
    const bool parentScriptMixin =
        rawParentScriptMixin.is<bool>() && rawParentScriptMixin.as<bool>();
    const sol::object rawLocalScriptMixin =
        classAttrs.raw_get<sol::object>("scriptMixin");
    const bool hasLocalScriptMixin =
        rawLocalScriptMixin.valid() &&
        rawLocalScriptMixin.get_type() != sol::type::lua_nil;
    if (hasLocalScriptMixin && !rawLocalScriptMixin.is<bool>()) {
        throw std::runtime_error("scriptMixin must be a boolean: " + classPath);
    }
    const bool scriptMixin = hasLocalScriptMixin
                                 ? rawLocalScriptMixin.as<bool>()
                                 : parentScriptMixin;
    const bool parentIsBlueprint = parentPath.starts_with("Data.Blueprints.");
    if (parentIsBlueprint && hasLocalScriptMixin &&
        scriptMixin != parentScriptMixin) {
        throw std::runtime_error(
            "Blueprint inheritance cannot mix ScriptMixin and graph modes: " +
            classPath + " -> " + parentPath);
    }
    const sol::object rawLocalScriptPath =
        classAttrs.raw_get<sol::object>("scriptPath");
    const bool hasLocalScriptPath =
        rawLocalScriptPath.valid() &&
        rawLocalScriptPath.get_type() != sol::type::lua_nil;
    if (hasLocalScriptPath && !rawLocalScriptPath.is<std::string>()) {
        throw std::runtime_error("scriptPath must be a string: " + classPath);
    }
    const std::string localScriptPath =
        hasLocalScriptPath ? rawLocalScriptPath.as<std::string>() : "";
    if (scriptMixin && !parentIsBlueprint && localScriptPath.empty()) {
        throw std::runtime_error(
            "Root ScriptMixin blueprint must declare scriptPath: " + classPath);
    }
    if (scriptMixin && hasLocalScriptPath && localScriptPath.empty()) {
        throw std::runtime_error("Local scriptPath must be non-empty: " +
                                 classPath);
    }

    applyConfigValues(lua, parentClass.as<sol::table>(), classAttrs,
                      configReferences(lua, parentClass.as<sol::table>()));
    TypedDataService& dataValues = typedDataService();
    const sol::object rawMetadata =
        luaValue(lua, dataValues.getAttrMetadata(runtimeValue(parentClass)));
    const sol::table attrMetadata = rawMetadata.is<sol::table>()
                                        ? rawMetadata.as<sol::table>()
                                        : lua.create_table();
    sol::table attrTypes = lua.create_table();

    sol::table definition = lua.create_table();
    sol::table instanceAttrs = lua.create_table();
    sol::object rawMixin = nilObject(lua);
    std::string normalizedScriptPath;
    if (scriptMixin && !localScriptPath.empty()) {
        normalizedScriptPath = normalizeScriptMixinPath(localScriptPath);
        sol::table mixin =
            loadScriptMixin(lua, classPath, normalizedScriptPath);
        mergeScriptMixin(lua, parentClassTable, mixin, definition,
                         instanceAttrs, classPath, normalizedScriptPath);
        rawMixin = sol::make_object(lua, mixin);
    }
    for (const auto& entry : classAttrs) {
        const sol::object mixinMember =
            definition.raw_get<sol::object>(entry.first);
        if (mixinMember.get_type() == sol::type::function) {
            throw std::runtime_error(
                "Blueprint attr cannot replace Mixin method '" +
                entry.first.as<std::string>() + "': " + classPath);
        }
        const sol::object rawFieldMetadata =
            attrMetadata.raw_get<sol::object>(entry.first);
        sol::object targetType = nilObject(lua);
        if (!rawFieldMetadata.is<sol::table>()) {
            if (entry.first.is<std::string>()) {
                targetType = luaValue(lua, dataValues.resolveAttrValueType(
                                               runtimeValue(parentClass),
                                               entry.first.as<std::string>()));
            }
            if (targetType.valid() &&
                targetType.get_type() != sol::type::lua_nil) {
                attrTypes.raw_set(entry.first, targetType);
            }
        }
        definition.raw_set(
            entry.first,
            cloneAttrValue(lua, parentClass.as<sol::table>(), entry.first,
                           entry.second, rawFieldMetadata, targetType));
        instanceAttrs.raw_set(
            entry.first,
            ludork::standard::class_runtime::deepCopy(lua, entry.second));
    }

    definition.raw_set("_GENERATED_CLASS", true);
    definition.raw_set("__blueprintClassPath", classPath);
    definition.raw_set(
        "init",
        sol::as_function([state = lua.lua_state(), classPath](
                             sol::object self, sol::variadic_args arguments) {
            initializeGeneratedInstance(state, classPath, self, arguments);
        }));
    sol::table bases = lua.create_table(1, 0);
    bases.raw_set(1, parentClass);
    sol::table generatedClass =
        ludork::standard::class_runtime::finalizeClass(definition, bases);
    targetClass = sol::make_object(lua, generatedClass);

    sol::table record = lua.create_table();
    const sol::object parentRecord =
        state.raw_get<sol::table>("records").raw_get<sol::object>(parentPath);
    record.raw_set("class", generatedClass);
    record.raw_set("attrs", instanceAttrs);
    record.raw_set("parent", parentClass);
    record.raw_set("parentRecord", parentRecord);
    record.raw_set("metadata", attrMetadata);
    record.raw_set("types", attrTypes);
    record.raw_set("scriptMixin", scriptMixin);
    if (rawMixin.is<sol::table>()) {
        record.raw_set("scriptTable", rawMixin);
    }
    if (!normalizedScriptPath.empty()) {
        record.raw_set("scriptPath", normalizedScriptPath);
    }
    record.raw_set(
        "parentInit",
        parentRecord.is<sol::table>()
            ? parentRecord.as<sol::table>().raw_get<sol::object>("parentInit")
            : parentClass.as<sol::table>().get<sol::object>("init"));
    if (!scriptMixin) {
        const sol::object graphTemplate =
            compileGraphTemplate(lua, data, targetClass);
        if (graphTemplate.valid() &&
            graphTemplate.get_type() != sol::type::lua_nil) {
            record.raw_set("graphTemplate", graphTemplate);
        }
    }
    record.raw_set("graphCompiled", true);
    state.raw_get<sol::table>("records").raw_set(classPath, record);
    classes.raw_set(classPath, generatedClass);
    return {generatedClass, data};
}

}  // namespace ludork::engine::class_runtime_detail
