#include <Runtime/EngineClassRuntime.hpp>

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LudorkCoreBinding.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <Utils/DataValue.hpp>

#include <sol2/sol.hpp>

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

namespace {

constexpr const char* CLASS_RESOLVER_STATE_KEY =
    "Ludork.Engine.classResolverState";
constexpr std::array<const char*, 5> CLASS_RUNTIME_SERVICE_NAMES{
    "nodegraph.resolveClass",          "nodegraph.classData",
    "nodegraph.instantiateClassGraph", "nodegraph.classGraphHasExecutableEvent",
    "nodegraph.invalidateClass",
};

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result,
                          int index = 0) {
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

template <typename... Arguments>
sol::protected_function_result callRaw(const sol::table& owner,
                                       const char* name,
                                       Arguments&&... arguments) {
    const sol::object rawFunction = owner.raw_get<sol::object>(name);
    if (!rawFunction.is<sol::protected_function>()) {
        throw std::runtime_error(std::string("Lua function is not defined: ") +
                                 name);
    }
    sol::protected_function function =
        rawFunction.as<sol::protected_function>();
    return function(std::forward<Arguments>(arguments)...);
}

template <typename... Arguments>
sol::object call(const sol::table& owner, const char* name,
                 Arguments&&... arguments) {
    sol::state_view lua(owner.lua_state());
    sol::protected_function_result result =
        callRaw(owner, name, std::forward<Arguments>(arguments)...);
    return checkedResult(lua, result);
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

sol::object callRuntimeServiceFirst(sol::state_view lua,
                                    const std::string& name,
                                    const std::vector<sol::object>& arguments) {
    sol::table packed = lua.create_table(static_cast<int>(arguments.size()), 1);
    packed.raw_set("n", arguments.size());
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        packed.raw_set(index + 1, arguments[index]);
    }
    const sol::table result =
        ludork::standard::class_runtime::callService(lua, name, packed);
    const sol::object value = result.raw_get<sol::object>(1);
    return value.valid() ? value : nilObject(lua);
}

sol::object compileGraphTemplate(sol::state_view lua, const sol::table& data,
                                 const sol::object& classType) {
    const sol::object graphData = data.raw_get<sol::object>("graph");
    if (!graphData.is<sol::table>()) {
        return nilObject(lua);
    }
    return callRuntimeServiceFirst(lua, "blueprint.compileGraph",
                                   {graphData, classType});
}

bool classGraphHasExecutableEvent(sol::state_view lua,
                                  const std::string& classPath,
                                  const std::string& eventName) {
    const sol::object rawClass =
        resolverState(lua).raw_get<sol::table>("classes").raw_get<sol::object>(
            classPath);
    if (rawClass.is<sol::table>()) {
        const sol::object rawScriptMixin =
            rawClass.as<sol::table>().get<sol::object>("scriptMixin");
        if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
            return false;
        }
    }
    const sol::object rawData = resolverState(lua)
                                    .raw_get<sol::table>("classData")
                                    .raw_get<sol::object>(classPath);
    if (!rawData.is<sol::table>()) {
        return false;
    }
    const sol::object rawGraph =
        rawData.as<sol::table>().raw_get<sol::object>("graph");
    if (!rawGraph.is<sol::table>()) {
        return false;
    }
    const sol::table graph = rawGraph.as<sol::table>();
    const sol::object rawNodeGraph = graph.raw_get<sol::object>("nodeGraph");
    const sol::object rawStartNodes = graph.raw_get<sol::object>("startNodes");
    if (!rawNodeGraph.is<sol::table>() || !rawStartNodes.is<sol::table>()) {
        return false;
    }
    const sol::object rawEvent =
        rawNodeGraph.as<sol::table>().raw_get<sol::object>(eventName);
    const sol::object rawStart =
        rawStartNodes.as<sol::table>().raw_get<sol::object>(eventName);
    if (!rawEvent.is<sol::table>() ||
        (!rawStart.is<std::int64_t>() && !rawStart.is<double>())) {
        return false;
    }
    const sol::object rawNodes =
        rawEvent.as<sol::table>().raw_get<sol::object>("nodes");
    const double start = rawStart.is<std::int64_t>()
                             ? static_cast<double>(rawStart.as<std::int64_t>())
                             : rawStart.as<double>();
    return rawNodes.is<sol::table>() && start >= 0.0 &&
           static_cast<std::size_t>(start) < rawNodes.as<sol::table>().size();
}

sol::object instantiateClassGraph(sol::state_view lua,
                                  const std::string& classPath,
                                  const sol::object& parent) {
    sol::table state = resolverState(lua);
    sol::table records = state.raw_get<sol::table>("records");
    sol::object rawRecord = records.raw_get<sol::object>(classPath);
    if (!rawRecord.is<sol::table>()) {
        resolveClass(lua, sol::make_object(lua, classPath), nilObject(lua));
        rawRecord = records.raw_get<sol::object>(classPath);
    }
    if (!rawRecord.is<sol::table>()) {
        return nilObject(lua);
    }
    sol::table record = rawRecord.as<sol::table>();
    const sol::object rawScriptMixin =
        record.raw_get<sol::object>("scriptMixin");
    if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
        return nilObject(lua);
    }
    sol::object graphTemplate = record.raw_get<sol::object>("graphTemplate");
    const sol::object rawGraphCompiled =
        record.raw_get<sol::object>("graphCompiled");
    if (!rawGraphCompiled.is<bool>() || !rawGraphCompiled.as<bool>()) {
        const sol::object rawData = state.raw_get<sol::table>("classData")
                                        .raw_get<sol::object>(classPath);
        const sol::object classType = record.raw_get<sol::object>("class");
        if (rawData.is<sol::table>()) {
            graphTemplate =
                compileGraphTemplate(lua, rawData.as<sol::table>(), classType);
        }
        if (graphTemplate.valid() &&
            graphTemplate.get_type() != sol::type::lua_nil) {
            record.raw_set("graphTemplate", graphTemplate);
        }
        record.raw_set("graphCompiled", true);
    }
    if (!graphTemplate.valid() ||
        graphTemplate.get_type() == sol::type::lua_nil) {
        return nilObject(lua);
    }
    return callRuntimeServiceFirst(lua, "blueprint.instantiateGraphTemplate",
                                   {graphTemplate, parent});
}

std::vector<sol::table> classMro(const sol::table& classTable) {
    std::vector<sol::table> result;
    sol::state_view lua(classTable.lua_state());
    const sol::table mro = ludork::standard::class_runtime::getMroCopy(
        lua, sol::make_object(lua, classTable));
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object current = mro.raw_get<sol::object>(index);
        if (current.is<sol::table>()) {
            result.push_back(current.as<sol::table>());
        }
    }
    if (result.empty()) {
        result.push_back(classTable);
    }
    return result;
}

bool isSequence(const sol::table& value) {
    const sol::object rawCount = value.raw_get<sol::object>("n");
    std::size_t count = value.size();
    if (rawCount.valid() && rawCount.get_type() != sol::type::lua_nil) {
        if (!rawCount.is<lua_Integer>() || rawCount.as<lua_Integer>() < 0) {
            return false;
        }
        count = static_cast<std::size_t>(rawCount.as<lua_Integer>());
    }
    std::size_t entries = 0;
    for (const auto& entry : value) {
        if (entry.first.is<std::string>() &&
            entry.first.as<std::string>() == "n") {
            continue;
        }
        if (!entry.first.is<lua_Integer>()) {
            return false;
        }
        const lua_Integer index = entry.first.as<lua_Integer>();
        if (index < 1 || static_cast<std::size_t>(index) > count) {
            return false;
        }
        ++entries;
    }
    return entries == count;
}

sol::table engineTable(sol::state_view lua) {
    const sol::object engine = lua.globals().raw_get<sol::object>("Engine");
    if (!engine.is<sol::table>()) {
        throw std::runtime_error("Engine module is not initialized");
    }
    return engine.as<sol::table>();
}

sol::table nestedTable(const sol::table& root,
                       std::initializer_list<const char*> path) {
    sol::object current = root;
    for (const char* name : path) {
        if (!current.is<sol::table>()) {
            throw std::runtime_error("Engine runtime table is unavailable");
        }
        current = current.as<sol::table>().raw_get<sol::object>(name);
    }
    if (!current.is<sol::table>()) {
        throw std::runtime_error("Engine runtime table is unavailable");
    }
    return current.as<sol::table>();
}

RuntimeValue runtimeValue(const sol::object& value) {
    return ludork_core::readLuaValue<RuntimeValue>(value);
}

sol::object luaValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork_core::writeLuaValue(lua, value);
}

std::string declaringModule(const sol::object& value) {
    return value.is<std::string>() ? value.as<std::string>() : std::string();
}

sol::object cloneMetadataValue(sol::state_view lua, const sol::object& value,
                               const sol::table& fieldMetadata,
                               const std::string& fallbackModule = {}) {
    DataValueService& dataValues = dataValueService();
    const sol::object typeReference =
        fieldMetadata.raw_get<sol::object>("type");
    const sol::object module = fieldMetadata.raw_get<sol::object>("module");
    const RuntimeValue runtimeType = runtimeValue(typeReference);
    const std::string fieldModule = declaringModule(module);
    const std::string moduleName =
        fieldModule.empty() ? fallbackModule : fieldModule;

    if (typeReference.is<std::string>()) {
        const std::string typeName = typeReference.as<std::string>();
        if (typeName == "bool" || typeName == "int" || typeName == "float" ||
            typeName == "string") {
            return luaValue(lua, dataValues.resolveTypedDataValue(
                                     runtimeValue(value), runtimeType,
                                     RuntimeValue::Map{}, moduleName));
        }
        if (typeName == "any" || typeName == "table" || typeName == "list" ||
            typeName == "dict" || typeName == "Pair" ||
            typeName.ends_with("[]")) {
            return ludork::standard::class_runtime::deepCopy(lua, value);
        }
    }

    const sol::object component =
        fieldMetadata.raw_get<sol::object>("component");
    if (component.is<bool>() && component.as<bool>()) {
        const RuntimeValue componentType =
            dataValues.resolveMetadataType(runtimeType, moduleName);
        if (!componentType.isNil()) {
            const RuntimeValue resolved =
                ludork::engine::components::componentFromData(
                    componentType, runtimeValue(value));
            return luaValue(lua, resolved);
        }
        return ludork::standard::class_runtime::deepCopy(lua, value);
    }
    if (value.is<sol::table>() && !isSequence(value.as<sol::table>())) {
        return ludork::standard::class_runtime::deepCopy(lua, value);
    }
    if (value.is<std::string>()) {
        return luaValue(lua,
                        dataValues.evalDataExpression(runtimeValue(value)));
    }
    return luaValue(
        lua, dataValues.resolveTypedDataValue(runtimeValue(value), runtimeType,
                                              RuntimeValue::Map{}, moduleName));
}

sol::object cloneAttrValue(sol::state_view lua, const sol::table& parentClass,
                           const sol::object& key, const sol::object& value,
                           const sol::object& rawMetadata,
                           const sol::object& rawTargetType) {
    DataValueService& dataValues = dataValueService();
    if (rawMetadata.is<sol::table>()) {
        return cloneMetadataValue(lua, value, rawMetadata.as<sol::table>());
    }
    RuntimeValue targetType;
    if (rawTargetType.valid() &&
        rawTargetType.get_type() != sol::type::lua_nil) {
        targetType = runtimeValue(rawTargetType);
    } else if (key.is<std::string>()) {
        targetType = dataValues.resolveAttrValueType(
            runtimeValue(sol::make_object(lua, parentClass)),
            key.as<std::string>());
    }
    if (value.is<std::string>()) {
        if (dataValues.shouldEvalValueType(targetType)) {
            return luaValue(lua,
                            dataValues.evalDataExpression(runtimeValue(value)));
        }
    }
    const std::string* targetName = targetType.getIf<std::string>();
    if (!targetType.isNil() &&
        (targetName == nullptr || *targetName != "any")) {
        return ludork::standard::class_runtime::deepCopy(
            lua, luaValue(lua, dataValues.resolveTypedDataValue(
                                   runtimeValue(value), targetType)));
    }
    return ludork::standard::class_runtime::deepCopy(lua, value);
}

sol::table configReferences(sol::state_view lua, const sol::table& owner) {
    sol::table cache =
        resolverState(lua).raw_get<sol::table>("configReferences");
    const sol::object cached = cache.raw_get<sol::object>(owner);
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    sol::table result = lua.create_table();
    DataValueService& dataValues = dataValueService();
    std::vector<sol::table> mro = classMro(owner);
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const RuntimeValue currentType =
            runtimeValue(sol::make_object(lua, *current));
        const RuntimeValue metadata =
            dataValues.getClassTypeMetadata(currentType).first;
        const RuntimeValue::Map* metadataFields =
            metadata.getIf<RuntimeValue::Map>();
        if (metadataFields == nullptr) {
            continue;
        }
        const auto metaIterator = metadataFields->find("Meta");
        if (metaIterator == metadataFields->end()) {
            continue;
        }
        for (const auto& [name, reference] :
             getConfigVars(metaIterator->second)) {
            result.raw_set(name, luaValue(lua, reference));
        }
    }
    cache.raw_set(owner, result);
    return result;
}

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

std::string normalizeScriptMixinPath(const std::string& value) {
    std::string path = value;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty() || path.front() == '/' ||
        path.find(':') != std::string::npos || !path.ends_with(".lua") ||
        path.ends_with("_meta.lua")) {
        throw std::runtime_error(
            "scriptPath must be a relative .lua path under Scripts/Mixins");
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string part = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == "." || part == "..") {
            throw std::runtime_error("scriptPath cannot leave Scripts/Mixins");
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return path;
}

std::string scriptMixinModuleName(const std::string& scriptPath) {
    std::string module =
        "Mixins." + scriptPath.substr(0, scriptPath.size() - 4);
    std::replace(module.begin(), module.end(), '/', '.');
    return module;
}

std::string findLuaModuleFile(sol::state_view lua,
                              const std::string& moduleName) {
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        throw std::runtime_error("Lua package table is unavailable");
    }
    const sol::table package = rawPackage.as<sol::table>();
    const sol::object rawSearch = package.raw_get<sol::object>("searchpath");
    const sol::object rawPath = package.raw_get<sol::object>("path");
    if (!rawSearch.is<sol::protected_function>() ||
        !rawPath.is<std::string>()) {
        throw std::runtime_error("Lua package.searchpath is unavailable");
    }
    sol::protected_function search = rawSearch.as<sol::protected_function>();
    sol::protected_function_result result = search(moduleName, rawPath);
    if (result.valid() && result.return_count() > 0) {
        const sol::object found = result.get<sol::object>();
        if (found.is<std::string>()) {
            return found.as<std::string>();
        }
    }
    throw std::runtime_error("Mixin script was not found: " + moduleName);
}

bool tableHasMetatable(sol::state_view lua, const sol::table& table) {
    lua_State* state = lua.lua_state();
    table.push();
    const bool hasMetatable = lua_getmetatable(state, -1) != 0;
    lua_pop(state, hasMetatable ? 2 : 1);
    return hasMetatable;
}

bool isScriptMixinReservedName(const std::string& name) {
    return name.starts_with("__") || name == "init" || name == "new" ||
           name == "scriptMixin" || name == "scriptPath" ||
           name == "_GENERATED_CLASS" || name == "_graph" ||
           name == "_hasImplementationOwner";
}

sol::table loadScriptMixin(sol::state_view lua, const std::string& classPath,
                           const std::string& scriptPath) {
    const std::string moduleName = scriptMixinModuleName(scriptPath);
    const std::string filePath = findLuaModuleFile(lua, moduleName);
    sol::load_result loaded = lua.load_file(filePath);
    if (!loaded.valid()) {
        const sol::error error = loaded;
        throw std::runtime_error("Failed to load Mixin " + scriptPath +
                                 " for " + classPath + ": " + error.what());
    }
    sol::protected_function chunk = loaded;
    sol::protected_function_result result = chunk();
    sol::object value;
    try {
        value = checkedResult(lua, result);
    } catch (const std::runtime_error& error) {
        throw std::runtime_error("Failed to execute Mixin " + scriptPath +
                                 " for " + classPath + ": " + error.what());
    }
    if (!value.is<sol::table>()) {
        throw std::runtime_error("Mixin " + scriptPath + " for " + classPath +
                                 " must return a table");
    }
    sol::table mixin = value.as<sol::table>();
    if (tableHasMetatable(lua, mixin)) {
        throw std::runtime_error("Mixin " + scriptPath + " for " + classPath +
                                 " must return a table without a metatable");
    }
    for (const auto& entry : mixin) {
        if (!entry.first.is<std::string>()) {
            throw std::runtime_error("Mixin " + scriptPath + " for " +
                                     classPath +
                                     " must use string member names");
        }
        const std::string name = entry.first.as<std::string>();
        if (isScriptMixinReservedName(name)) {
            throw std::runtime_error("Mixin " + scriptPath + " for " +
                                     classPath + " uses reserved member '" +
                                     name + "'");
        }
    }
    return mixin;
}

void mergeScriptMixin(sol::state_view lua, const sol::table& parentClass,
                      const sol::table& mixin, sol::table definition,
                      sol::table instanceAttrs, const std::string& classPath,
                      const std::string& scriptPath) {
    for (const auto& entry : mixin) {
        const std::string name = entry.first.as<std::string>();
        const sol::object inherited = parentClass.get<sol::object>(name);
        const bool valueIsFunction =
            entry.second.get_type() == sol::type::function;
        const bool inheritedExists =
            inherited.valid() && inherited.get_type() != sol::type::lua_nil;
        if (inheritedExists &&
            (inherited.get_type() == sol::type::function) != valueIsFunction) {
            throw std::runtime_error(
                "Mixin " + scriptPath + " for " + classPath +
                " changes the member kind of '" + name + "'");
        }
        if (valueIsFunction) {
            definition.raw_set(name, entry.second);
        } else {
            definition.raw_set(name, ludork::standard::class_runtime::deepCopy(
                                         lua, entry.second));
            instanceAttrs.raw_set(
                name,
                ludork::standard::class_runtime::deepCopy(lua, entry.second));
        }
    }
}

sol::object resolveConfigValue(sol::state_view lua, const sol::object& value,
                               const sol::table& reference) {
    if (!value.is<std::string>() || !value.as<std::string>().empty()) {
        return value;
    }
    const sol::object rawConfig = reference.raw_get<sol::object>(1);
    const sol::object rawSetting = reference.raw_get<sol::object>(2);
    if (!rawConfig.is<std::string>() || !rawSetting.is<std::string>()) {
        return value;
    }
    const std::vector<RuntimeValue> resolved = resolveRuntime(
        "config.resolve", {RuntimeValue(rawConfig.as<std::string>()),
                           RuntimeValue(rawSetting.as<std::string>())});
    if (resolved.empty() ||
        !std::holds_alternative<std::string>(resolved.front().storage())) {
        return value;
    }
    return sol::make_object(lua,
                            std::get<std::string>(resolved.front().storage()));
}

void applyConfigValues(sol::state_view lua, const sol::table& parentClass,
                       sol::table classAttrs, const sol::table& references) {
    for (const auto& entry : references) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        const std::string name = entry.first.as<std::string>();
        const sol::object current = classAttrs.raw_get<sol::object>(name);
        if (current.valid() && current.get_type() != sol::type::lua_nil) {
            classAttrs.raw_set(
                name, resolveConfigValue(lua, current,
                                         entry.second.as<sol::table>()));
            continue;
        }
        sol::object parentValue = parentClass.get<sol::object>(name);
        if (!parentValue.valid() ||
            parentValue.get_type() == sol::type::lua_nil) {
            parentValue = sol::make_object(lua, std::string());
        }
        const sol::object resolved =
            resolveConfigValue(lua, parentValue, entry.second.as<sol::table>());
        if (!ludork::standard::class_runtime::rawEqual(parentValue, resolved)) {
            classAttrs.raw_set(name, resolved);
        }
    }
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
                                                  const sol::object& rawRoot);

void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const sol::object& self,
                                 const sol::variadic_args& arguments) {
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    sol::state_view lua(state);
    const sol::table records =
        resolverState(lua).raw_get<sol::table>("records");
    sol::object rawRecord = records.raw_get<sol::object>(classPath);
    if (!rawRecord.is<sol::table>()) {
        return;
    }
    const sol::table record = rawRecord.as<sol::table>();
    std::unordered_set<std::string> appliedAttrs;
    while (rawRecord.is<sol::table>()) {
        const sol::table current = rawRecord.as<sol::table>();
        const sol::table classAttrs = current.raw_get<sol::table>("attrs");
        const sol::table parentClass = current.raw_get<sol::table>("parent");
        const sol::table attrMetadata = current.raw_get<sol::table>("metadata");
        const sol::table attrTypes = current.raw_get<sol::table>("types");
        for (const auto& entry : classAttrs) {
            if (entry.first.is<std::string>() &&
                appliedAttrs.insert(entry.first.as<std::string>()).second &&
                !ludork::standard::class_runtime::hasOwnField(lua, self,
                                                              entry.first)) {
                ludork::standard::class_runtime::protectedSet(
                    lua, self, entry.first,
                    cloneAttrValue(
                        lua, parentClass, entry.first, entry.second,
                        attrMetadata.raw_get<sol::object>(entry.first),
                        attrTypes.raw_get<sol::object>(entry.first)));
            }
        }
        rawRecord = current.raw_get<sol::object>("parentRecord");
    }
    const sol::object rawInit = record.raw_get<sol::object>("parentInit");
    if (rawInit.is<sol::protected_function>()) {
        std::vector<sol::object> values;
        values.reserve(arguments.size() + 1);
        values.push_back(self);
        for (const auto& argument : arguments) {
            values.push_back(argument.get<sol::object>());
        }
        sol::protected_function init = rawInit.as<sol::protected_function>();
        sol::protected_function_result result = init(sol::as_args(values));
        checkedResult(lua, result);
    }
    ludork::engine::components::normaliseInstanceComponents(
        ludork_core::readLuaValue<RuntimeValue>(self));
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
        rawData = callRuntimeServiceFirst(lua, "blueprint.classDataByPath",
                                          {sol::make_object(lua, classPath)});
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
        const sol::table coreSystem = requireTable(lua, "CoreSystem");
        const sol::object exists = call(coreSystem, "exists", filePath);
        if (!exists.is<bool>() || !exists.as<bool>()) {
            throw std::runtime_error("Class " + classPath + " not found");
        }

        const sol::table file =
            nestedTable(engineTable(lua), {"Utils", "File"});
        rawData = call(file, "getJSONData", filePath);
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
    DataValueService& dataValues = dataValueService();
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

void registerService(sol::state_view lua, const std::string& name,
                     sol::object callback) {
    if (!callback.is<sol::protected_function>()) {
        throw std::invalid_argument("Runtime service callback is not callable");
    }
    ludork::standard::class_runtime::registerService(
        lua, name, callback.as<sol::protected_function>());
}

}  // namespace

void initializeEngineClassRuntime(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    lua.registry().raw_set(CLASS_RESOLVER_STATE_KEY, sol::lua_nil);
    resolverState(lua);
    const sol::object rawDefaultResolver = sol::make_object(
        lua,
        sol::as_function([state](sol::object value, sol::table fieldMetadata,
                                 sol::object rawModule) {
            ludork::standard::LuaExecutionScope execution(state);
            sol::state_view callbackLua(state);
            if (!execution.active()) {
                return nilObject(callbackLua);
            }
            return cloneMetadataValue(callbackLua, value, fieldMetadata,
                                      declaringModule(rawModule));
        }));
    ludork::standard::class_runtime::registerNativeClassDefaultResolver(
        lua, rawDefaultResolver.as<sol::protected_function>());
    registerService(
        lua, CLASS_RUNTIME_SERVICE_NAMES[0],
        sol::make_object(
            lua,
            sol::as_function([state](sol::object classPath, sol::object root) {
                ludork::standard::LuaExecutionScope execution(state);
                if (!execution.active()) {
                    return std::tuple<sol::object, sol::object>{
                        sol::make_object(sol::state_view(state), sol::lua_nil),
                        sol::make_object(sol::state_view(state), sol::lua_nil)};
                }
                return resolveClass(sol::state_view(state), classPath, root);
            })));
    registerService(
        lua, CLASS_RUNTIME_SERVICE_NAMES[1],
        sol::make_object(
            lua, sol::as_function([state](const std::string& path) {
                ludork::standard::LuaExecutionScope execution(state);
                sol::state_view callbackLua(state);
                if (!execution.active()) {
                    return nilObject(callbackLua);
                }
                return resolverState(callbackLua)
                    .raw_get<sol::table>("classData")
                    .raw_get<sol::object>(path);
            })));
    registerService(
        lua, CLASS_RUNTIME_SERVICE_NAMES[2],
        sol::make_object(
            lua, sol::as_function([state](const std::string& path,
                                          sol::object parent) {
                ludork::standard::LuaExecutionScope execution(state);
                sol::state_view callbackLua(state);
                if (!execution.active()) {
                    return nilObject(callbackLua);
                }
                return instantiateClassGraph(callbackLua, path, parent);
            })));
    registerService(
        lua, CLASS_RUNTIME_SERVICE_NAMES[3],
        sol::make_object(
            lua, sol::as_function([state](const std::string& path,
                                          const std::string& eventName) {
                ludork::standard::LuaExecutionScope execution(state);
                if (!execution.active()) {
                    return false;
                }
                return classGraphHasExecutableEvent(sol::state_view(state),
                                                    path, eventName);
            })));
    registerService(
        lua, CLASS_RUNTIME_SERVICE_NAMES[4],
        sol::make_object(
            lua, sol::as_function([state](const std::string& path) {
                ludork::standard::LuaExecutionScope execution(state);
                if (!execution.active()) {
                    return;
                }
                sol::state_view callbackLua(state);
                callRuntimeServiceFirst(callbackLua,
                                        "blueprint.invalidateClassData",
                                        {sol::make_object(callbackLua, path)});
                sol::table resolver = resolverState(callbackLua);
                const sol::object rawRecord =
                    resolver.raw_get<sol::table>("records")
                        .raw_get<sol::object>(path);
                if (rawRecord.is<sol::table>()) {
                    rawRecord.as<sol::table>().raw_set("graphTemplate",
                                                       sol::lua_nil);
                    rawRecord.as<sol::table>().raw_set("graphCompiled", false);
                }
                resolver.raw_get<sol::table>("classes").raw_set(path,
                                                                sol::lua_nil);
                resolver.raw_get<sol::table>("classData")
                    .raw_set(path, sol::lua_nil);
                resolver.raw_get<sol::table>("records").raw_set(path,
                                                                sol::lua_nil);
            })));
}

void shutdownEngineClassRuntime(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    ludork::standard::class_runtime::unregisterNativeClassDefaultResolver(lua);
    for (const char* name : CLASS_RUNTIME_SERVICE_NAMES) {
        ludork::standard::class_runtime::unregisterService(lua, name);
    }
    lua.registry().raw_set(CLASS_RESOLVER_STATE_KEY, sol::lua_nil);
}
