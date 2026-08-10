#include <Runtime/EngineRuntimeServices.hpp>

#include "RuntimeSubsystemServices.hpp"

#include <ClassServices.hpp>
#include <Gameplay/Components/ComponentRuntime.hpp>
#include <LuaError.hpp>
#include <LudorkCoreBinding.hpp>
#include <NodeGraph/Graph.hpp>
#include <Runtime/EngineClassRuntime.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>
#include <Utils/DataValue.hpp>
#include <Utils/EventBus.hpp>
#include <Utils/Inner.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

constexpr const char* BLUEPRINT_IMPLEMENTATION_CACHE_KEY =
    "Ludork.Engine.blueprintImplementationCache";
constexpr const char* NODEGRAPH_REF_LOCALS_KEY =
    "Ludork.Engine.NodeGraph.refLocals";
constexpr const char* NODEGRAPH_CONTEXTS_KEY =
    "Ludork.Engine.NodeGraph.contexts";
constexpr const char* CLASS_IDENTITY_CACHE_KEY =
    "Ludork.Engine.classIdentityCache";
constexpr const char* CLASS_TYPE_METADATA_CACHE_KEY =
    "Ludork.Engine.classTypeMetadataCache";
constexpr const char* ATTR_METADATA_CACHE_KEY =
    "Ludork.Engine.attrMetadataCache";
constexpr const char* COMPONENT_CACHES_KEY = "Ludork.Engine.componentCaches";

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

std::function<void()> completionCallback(const sol::object& value) {
    if (!value.valid() || value.get_type() == sol::type::none ||
        value.get_type() == sol::type::lua_nil) {
        return {};
    }
    return ludork_core::readLuaValue<std::function<void()>>(value);
}

void invokeCompletion(const std::function<void()>& callback) {
    if (callback) {
        callback();
    }
}

class CompletionBarrier {
public:
    CompletionBarrier(std::size_t count, std::function<void()> callback)
        : remaining_(count), callback_(std::move(callback)) {}

    void complete() {
        if (remaining_ == 0) {
            return;
        }
        --remaining_;
        if (remaining_ == 0) {
            invokeCompletion(callback_);
        }
    }

private:
    std::size_t remaining_;
    std::function<void()> callback_;
};

sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    return ludork::standard::class_runtime::protectedGet(lua, target, key);
}

void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value) {
    ludork::standard::class_runtime::protectedSet(lua, target, key, value);
}

sol::table createWeakTable(sol::state_view lua, const char* mode) {
    sol::table result = lua.create_table();
    sol::table metatable = lua.create_table();
    metatable["__mode"] = mode;
    result[sol::metatable_key] = metatable;
    return result;
}

sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode) {
    sol::table registry = lua.registry();
    const sol::object value = registry.raw_get<sol::object>(key);
    if (value.is<sol::table>()) {
        return value.as<sol::table>();
    }
    sol::table result = weakMode == nullptr ? lua.create_table()
                                            : createWeakTable(lua, weakMode);
    registry.raw_set(key, result);
    return result;
}

sol::table componentCache(sol::state_view lua, const sol::object& rawKind) {
    if (!rawKind.is<std::string>()) {
        throw std::invalid_argument("Component cache kind must be a string");
    }
    const std::string kind = rawKind.as<std::string>();
    if (kind != "types" && kind != "fieldDefaults" && kind != "fieldMap" &&
        kind != "inheritedDefaults") {
        throw std::invalid_argument("Unknown component cache kind: " + kind);
    }
    sol::table caches = registryTable(lua, COMPONENT_CACHES_KEY);
    const sol::object rawCache = caches.raw_get<sol::object>(kind);
    if (rawCache.is<sol::table>()) {
        return rawCache.as<sol::table>();
    }
    sol::table cache = createWeakTable(lua, "k");
    caches.raw_set(kind, cache);
    return cache;
}

bool rawBool(const sol::table& table, const char* name) {
    const sol::object value = table.raw_get<sol::object>(name);
    return value.is<bool>() && value.as<bool>();
}

bool isClass(const sol::table& value) {
    const sol::object marker = value.raw_get<sol::object>("__ludorkClass");
    return marker.is<bool>() && marker.as<bool>();
}

sol::table objectMetatable(sol::state_view lua, const sol::object& value) {
    lua_State* state = lua.lua_state();
    value.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    sol::table result = sol::stack::get<sol::table>(state, -1);
    lua_pop(state, 2);
    return result;
}

bool isNativeType(sol::state_view lua, const sol::table& value) {
    return !isClass(value) && objectMetatable(lua, sol::make_object(lua, value))
                                  .raw_get<sol::object>("__type")
                                  .is<sol::table>();
}

bool isInstance(sol::this_state state, const sol::object& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isInstanceOf(sol::state_view(state),
                                                         value, targetClass);
}

bool isSubclass(sol::this_state state, const sol::table& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(sol::state_view(state),
                                                         value, targetClass);
}

sol::object classType(sol::this_state state, const sol::object& value) {
    return ludork::standard::class_runtime::typeOf(sol::state_view(state),
                                                   value);
}

bool rawEqual(sol::state_view lua, const sol::object& left,
              const sol::object& right) {
    static_cast<void>(lua);
    return ludork::standard::class_runtime::rawEqual(left, right);
}

sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result) {
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    return result.return_count() == 0 ? nilObject(lua)
                                      : result.get<sol::object>();
}

sol::table requireLuaTable(sol::state_view lua, const char* moduleName) {
    const sol::object loaded =
        ludork::standard::class_runtime::requireModule(lua, moduleName);
    if (!loaded.is<sol::table>()) {
        throw std::runtime_error(
            std::string("Lua module did not return a table: ") + moduleName);
    }
    return loaded.as<sol::table>();
}

bool luaBoolean(const sol::object& value) {
    return value.is<bool>() && value.as<bool>();
}

bool hasBlueprintEvent(sol::this_state state, const sol::object& object,
                       const std::string& eventName);

void dispatchBlueprintEvent(sol::this_state state, const sol::object& object,
                            const sol::object& rawObjectType,
                            const std::string& eventName,
                            const sol::object& rawKeywordArguments,
                            const std::function<void()>& onComplete);

void validateBlueprintEvent(sol::this_state state, const sol::object& object,
                            const std::string& eventName) {
    sol::state_view lua(state);
    if (!object.valid() || object.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Blueprint event target object is nil");
    }
    if (!hasBlueprintEvent(state, object, eventName)) {
        throw std::invalid_argument("Object has no blueprint event '" +
                                    eventName + "'");
    }
}

void invokeBlueprintEvent(sol::this_state state, const sol::object& object,
                          const std::string& eventName) {
    sol::state_view lua(state);
    dispatchBlueprintEvent(state, object, classType(state, object), eventName,
                           sol::make_object(lua, lua.create_table()), {});
}

sol::table runtimeResolverResult(sol::state_view lua,
                                 const std::vector<sol::object>& values) {
    sol::table result = lua.create_table(static_cast<int>(values.size()), 1);
    result.raw_set("n", values.size());
    std::size_t index = 1;
    for (const sol::object& value : values) {
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            result.raw_set(index, value);
        }
        ++index;
    }
    return result;
}

sol::object runtimeResolverArgument(sol::state_view lua,
                                    const sol::table& arguments,
                                    std::size_t index) {
    const sol::object value = arguments.raw_get<sol::object>(index);
    return value.valid() ? value : nilObject(lua);
}

sol::table callRegisteredRuntimeService(
    sol::state_view lua, const std::string& name,
    const std::vector<sol::object>& arguments) {
    return ludork::standard::class_runtime::callService(
        lua, name, runtimeResolverResult(lua, arguments));
}

struct RuntimeClassIdentity {
    sol::table descriptor;
    std::string module;
    std::string type;
    bool direct = false;
};

RuntimeClassIdentity classIdentityFromDescriptor(const sol::table& descriptor) {
    const sol::object rawModule = descriptor.raw_get<sol::object>("module");
    const sol::object rawType = descriptor.raw_get<sol::object>("type");
    return {
        descriptor,
        rawModule.is<std::string>() ? rawModule.as<std::string>()
                                    : std::string(),
        rawType.is<std::string>() ? rawType.as<std::string>() : std::string(),
        rawBool(descriptor, "direct")};
}

std::optional<RuntimeClassIdentity> resolveRuntimeClassIdentity(
    sol::state_view lua, const sol::object& classReference) {
    if (!classReference.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::table classTable = classReference.as<sol::table>();
    sol::table cache = registryTable(lua, CLASS_IDENTITY_CACHE_KEY, "k");
    const sol::object cached = cache.raw_get<sol::object>(classReference);
    if (cached.is<sol::table>()) {
        return classIdentityFromDescriptor(cached.as<sol::table>());
    }

    const sol::object explicitModule =
        classTable.raw_get<sol::object>("__metadataModule");
    if (explicitModule.is<std::string>() &&
        !explicitModule.as<std::string>().empty()) {
        sol::table descriptor = lua.create_table();
        descriptor.raw_set("module", explicitModule);
        descriptor.raw_set("direct", false);
        cache.raw_set(classReference, descriptor);
        return classIdentityFromDescriptor(descriptor);
    }

    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::object rawLoaded =
        rawPackage.as<sol::table>().raw_get<sol::object>("loaded");
    if (!rawLoaded.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::table loaded = rawLoaded.as<sol::table>();

    std::vector<std::string> directModules;
    for (const auto& entry : loaded) {
        if (entry.first.is<std::string>() &&
            rawEqual(lua, entry.second, classReference)) {
            directModules.push_back(entry.first.as<std::string>());
        }
    }
    std::sort(directModules.begin(), directModules.end());
    directModules.erase(std::unique(directModules.begin(), directModules.end()),
                        directModules.end());
    if (directModules.size() > 1) {
        throw std::runtime_error("Lua class is returned by multiple modules: " +
                                 directModules.front() + ", " +
                                 directModules.back());
    }
    if (!directModules.empty()) {
        sol::table descriptor = lua.create_table();
        descriptor.raw_set("module", directModules.front());
        descriptor.raw_set("direct", true);
        cache.raw_set(classReference, descriptor);
        return classIdentityFromDescriptor(descriptor);
    }

    std::vector<std::pair<std::string, std::string>> exports;
    for (const auto& entry : loaded) {
        if (!entry.first.is<std::string>() || !entry.second.is<sol::table>()) {
            continue;
        }
        const std::string moduleName = entry.first.as<std::string>();
        for (const auto& member : entry.second.as<sol::table>()) {
            if (!member.first.is<std::string>()) {
                continue;
            }
            const std::string exportName = member.first.as<std::string>();
            if (exportName.rfind("__", 0) == 0 ||
                exportName == "_hasImplementationOwner") {
                continue;
            }
            if (rawEqual(lua, member.second, classReference)) {
                exports.emplace_back(moduleName, exportName);
            }
        }
    }
    std::sort(exports.begin(), exports.end());
    exports.erase(std::unique(exports.begin(), exports.end()), exports.end());
    if (exports.size() > 1) {
        throw std::runtime_error(
            "Lua class has multiple module exports: " + exports.front().first +
            "." + exports.front().second + ", " + exports.back().first + "." +
            exports.back().second);
    }
    if (exports.empty()) {
        return std::nullopt;
    }

    sol::table descriptor = lua.create_table();
    descriptor.raw_set("module", exports.front().first);
    descriptor.raw_set("type", exports.front().second);
    descriptor.raw_set("direct", false);
    cache.raw_set(classReference, descriptor);
    return classIdentityFromDescriptor(descriptor);
}

sol::object findRuntimeClassModule(sol::state_view lua,
                                   const sol::object& classReference) {
    const std::optional<RuntimeClassIdentity> identity =
        resolveRuntimeClassIdentity(lua, classReference);
    return identity.has_value() && !identity->module.empty()
               ? sol::make_object(lua, identity->module)
               : nilObject(lua);
}

sol::object syntheticRuntimeMetadata(sol::state_view lua,
                                     const sol::table& classTable) {
    const sol::object native =
        classTable.raw_get<sol::object>("__runtimeMetadata");
    if (native.is<sol::table>()) {
        return native;
    }
    const sol::object rawTypes = classTable.raw_get<sol::object>("__types");
    const sol::object rawMeta =
        classTable.raw_get<sol::object>("__runtimeMeta");
    if (!rawTypes.is<sol::table>() && !rawMeta.is<sol::table>()) {
        return nilObject(lua);
    }

    sol::table metadata = lua.create_table();
    sol::table attrs = lua.create_table();
    std::size_t index = 1;
    if (rawTypes.is<sol::table>()) {
        for (const auto& entry : rawTypes.as<sol::table>()) {
            if (!entry.first.is<std::string>()) {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            attrs.raw_set(index++, name);
            sol::table member = lua.create_table();
            member.raw_set("type", entry.second);
            metadata.raw_set(name, member);
        }
    }
    metadata.raw_set("attrs", attrs);
    if (rawMeta.is<sol::table>()) {
        metadata.raw_set("Meta", rawMeta);
    }
    return sol::make_object(lua, metadata);
}

std::vector<sol::table> runtimeClassMro(sol::state_view lua,
                                        const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = ludork::standard::class_runtime::getMroCopy(
        lua, sol::make_object(lua, classTable));
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object value = mro.raw_get<sol::object>(index);
        if (value.is<sol::table>()) {
            result.push_back(value.as<sol::table>());
        }
    }
    if (result.empty()) {
        result.push_back(classTable);
    }
    return result;
}

sol::object runtimeTypeMetadata(sol::state_view lua,
                                const sol::table& classType);

sol::table collectRuntimeAttrMetadata(sol::state_view lua,
                                      const sol::table& owner) {
    sol::table cache = registryTable(lua, ATTR_METADATA_CACHE_KEY, "k");
    const sol::object ownerObject = sol::make_object(lua, owner);
    const sol::object cached = cache.raw_get<sol::object>(ownerObject);
    if (cached.is<sol::table>()) {
        return cached.as<sol::table>();
    }
    sol::table result = lua.create_table();
    const std::vector<sol::table> mro = runtimeClassMro(lua, owner);
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        sol::object rawMetadata = syntheticRuntimeMetadata(lua, *current);
        if (!rawMetadata.is<sol::table>()) {
            rawMetadata = runtimeTypeMetadata(lua, *current);
        }
        if (!rawMetadata.is<sol::table>()) {
            continue;
        }
        const sol::table metadata = rawMetadata.as<sol::table>();
        const sol::object rawAttrs = metadata.raw_get<sol::object>("attrs");
        if (!rawAttrs.is<sol::table>()) {
            continue;
        }
        const sol::object module =
            findRuntimeClassModule(lua, sol::make_object(lua, *current));
        const sol::table attrs = rawAttrs.as<sol::table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const sol::object rawName = attrs.raw_get<sol::object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const std::string name = rawName.as<std::string>();
            const sol::object rawMember = metadata.raw_get<sol::object>(name);
            if (!rawMember.is<sol::table>()) {
                continue;
            }
            const sol::table member = rawMember.as<sol::table>();
            const sol::object type = member.raw_get<sol::object>("type");
            if (type.get_type() == sol::type::lua_nil) {
                continue;
            }
            sol::table descriptor = lua.create_table();
            descriptor.raw_set("type", type);
            const sol::object component =
                member.raw_get<sol::object>("component");
            descriptor.raw_set("component",
                               component.is<bool>() && component.as<bool>());
            const sol::object declaredModule =
                member.raw_get<sol::object>("module");
            if (declaredModule.is<std::string>()) {
                descriptor.raw_set("module", declaredModule);
            } else if (module.is<std::string>()) {
                descriptor.raw_set("module", module);
            }
            descriptor.raw_set("metadata", member);
            result.raw_set(name, descriptor);
        }
    }
    for (auto current = mro.rbegin(); current != mro.rend(); ++current) {
        const sol::object rawTypes = current->raw_get<sol::object>("__types");
        if (!rawTypes.is<sol::table>()) {
            continue;
        }
        const sol::object module =
            findRuntimeClassModule(lua, sol::make_object(lua, *current));
        for (const auto& entry : rawTypes.as<sol::table>()) {
            if (!entry.first.is<std::string>()) {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            const sol::object existing = result.raw_get<sol::object>(name);
            sol::table descriptor = lua.create_table();
            descriptor.raw_set("type", entry.second);
            if (existing.is<sol::table>()) {
                const sol::table previous = existing.as<sol::table>();
                const sol::object component =
                    previous.raw_get<sol::object>("component");
                descriptor.raw_set(
                    "component", component.is<bool>() && component.as<bool>());
                const sol::object previousModule =
                    previous.raw_get<sol::object>("module");
                if (previousModule.is<std::string>()) {
                    descriptor.raw_set("module", previousModule);
                }
                const sol::object metadata =
                    previous.raw_get<sol::object>("metadata");
                if (metadata.get_type() != sol::type::lua_nil) {
                    descriptor.raw_set("metadata", metadata);
                }
            } else {
                descriptor.raw_set("component", false);
                if (module.is<std::string>()) {
                    descriptor.raw_set("module", module);
                }
            }
            result.raw_set(name, descriptor);
        }
    }
    cache.raw_set(ownerObject, result);
    return result;
}

std::pair<sol::object, sol::object> resolveRuntimeConfigVar(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName) {
    if (!rawName.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    sol::object rawClass = owner;
    if (!rawClass.is<sol::table>()) {
        rawClass = ludork::standard::class_runtime::typeOf(lua, owner);
    }
    if (!rawClass.is<sol::table>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string name = rawName.as<std::string>();
    for (const sol::table& current :
         runtimeClassMro(lua, rawClass.as<sol::table>())) {
        sol::object metadata = syntheticRuntimeMetadata(lua, current);
        if (!metadata.is<sol::table>()) {
            metadata = runtimeTypeMetadata(lua, current);
        }
        if (!metadata.is<sol::table>()) {
            continue;
        }
        const RuntimeValue metadataValue =
            ludork_core::readLuaValue<RuntimeValue>(metadata);
        const RuntimeValue::Map* metadataFields =
            metadataValue.getIf<RuntimeValue::Map>();
        if (metadataFields == nullptr) {
            continue;
        }
        const auto metaIterator = metadataFields->find("Meta");
        if (metaIterator == metadataFields->end()) {
            continue;
        }
        const RuntimeValue::Map references =
            getConfigVars(metaIterator->second);
        const auto iterator = references.find(name);
        if (iterator == references.end()) {
            continue;
        }
        const RuntimeValue::Array* reference =
            iterator->second.getIf<RuntimeValue::Array>();
        if (reference == nullptr || reference->size() < 2) {
            continue;
        }
        return {
            ludork_core::writeLuaValue(lua, (*reference)[0]),
            ludork_core::writeLuaValue(lua, (*reference)[1]),
        };
    }
    return {nilObject(lua), nilObject(lua)};
}

std::pair<sol::object, sol::object> resolveRuntimeMemberMetadata(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName) {
    if (!rawName.is<std::string>()) {
        return {nilObject(lua), nilObject(lua)};
    }
    const std::string name = rawName.as<std::string>();
    const sol::table mro =
        ludork::standard::class_runtime::getMroCopy(lua, owner);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawClass = mro.raw_get<sol::object>(index);
        if (!rawClass.is<sol::table>()) {
            continue;
        }
        sol::object metadata =
            syntheticRuntimeMetadata(lua, rawClass.as<sol::table>());
        if (!metadata.is<sol::table>()) {
            metadata = runtimeTypeMetadata(lua, rawClass.as<sol::table>());
        }
        if (!metadata.is<sol::table>()) {
            continue;
        }
        const sol::object member =
            metadata.as<sol::table>().raw_get<sol::object>(name);
        if (!member.is<sol::table>()) {
            continue;
        }
        const sol::object module = findRuntimeClassModule(lua, rawClass);
        return {member, module.valid() ? module : nilObject(lua)};
    }
    return {nilObject(lua), nilObject(lua)};
}

sol::object resolveRuntimePath(sol::state_view lua, const sol::object& root,
                               const std::string& path) {
    sol::object current = root;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('.', start);
        const std::string name = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (!current.is<sol::table>()) {
            return nilObject(lua);
        }
        current = current.as<sol::table>().get<sol::object>(name);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return current.valid() ? current : nilObject(lua);
}

std::optional<std::string> directRuntimeMetadataTypeName(
    sol::state_view lua, const std::string& modulePath) {
    const std::string metadataModule = modulePath + "_meta";
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return std::nullopt;
    }
    const sol::table package = rawPackage.as<sol::table>();
    const sol::object rawSearchPath =
        package.raw_get<sol::object>("searchpath");
    const sol::object rawPath = package.raw_get<sol::object>("path");
    if (!rawSearchPath.is<sol::protected_function>() ||
        !rawPath.is<std::string>()) {
        return std::nullopt;
    }
    sol::protected_function searchPath =
        rawSearchPath.as<sol::protected_function>();
    sol::protected_function_result searched =
        searchPath(metadataModule, rawPath);
    const sol::object found = checkedResult(lua, searched);
    if (!found.is<std::string>()) {
        return std::nullopt;
    }

    const sol::table metadata = requireLuaTable(lua, metadataModule.c_str());
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

sol::object requireRuntimeType(sol::state_view lua,
                               const std::string& modulePath,
                               const std::string& typeName) {
    const sol::object rawRequire =
        lua.globals().raw_get<sol::object>("require");
    if (!rawRequire.is<sol::protected_function>()) {
        return nilObject(lua);
    }
    sol::protected_function require = rawRequire.as<sol::protected_function>();
    sol::protected_function_result loaded = require(modulePath);
    const sol::object module = checkedResult(lua, loaded);
    if (!module.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table table = module.as<sol::table>();
    if (isClass(table)) {
        const std::size_t separator = modulePath.find_last_of('.');
        const std::string moduleType = separator == std::string::npos
                                           ? modulePath
                                           : modulePath.substr(separator + 1);
        if (typeName == moduleType) {
            return module;
        }
        const std::optional<std::string> metadataType =
            directRuntimeMetadataTypeName(lua, modulePath);
        return metadataType.has_value() && *metadataType == typeName
                   ? module
                   : nilObject(lua);
    }
    return resolveRuntimePath(lua, module, typeName);
}

sol::object resolveRuntimeMetadataType(sol::state_view lua,
                                       const sol::object& typeReference,
                                       const sol::object& declaringModule) {
    if (typeReference.is<sol::table>()) {
        const sol::table reference = typeReference.as<sol::table>();
        const sol::object module = reference.raw_get<sol::object>(1);
        const sol::object name = reference.raw_get<sol::object>(2);
        if (!module.is<std::string>() || !name.is<std::string>()) {
            return nilObject(lua);
        }
        return requireRuntimeType(lua, module.as<std::string>(),
                                  name.as<std::string>());
    }
    if (!typeReference.is<std::string>()) {
        return nilObject(lua);
    }
    const std::string name = typeReference.as<std::string>();
    const sol::object global =
        resolveRuntimePath(lua, sol::make_object(lua, lua.globals()), name);
    if (global.get_type() != sol::type::lua_nil) {
        return global;
    }
    if (declaringModule.is<std::string>() &&
        name.find('.') == std::string::npos) {
        return requireRuntimeType(lua, declaringModule.as<std::string>(), name);
    }
    return nilObject(lua);
}

bool runtimeSequence(const sol::table& table,
                     std::vector<sol::object>& values) {
    const sol::object rawLength = table.raw_get<sol::object>("n");
    const bool packed = rawLength.is<lua_Integer>();
    if (packed && rawLength.as<lua_Integer>() < 0) {
        return false;
    }
    const std::size_t length =
        packed ? static_cast<std::size_t>(rawLength.as<lua_Integer>())
               : table.size();
    std::size_t itemCount = 0;
    for (const auto& entry : table) {
        if (packed && entry.first.is<std::string>() &&
            entry.first.as<std::string>() == "n") {
            continue;
        }
        if (!entry.first.is<std::int64_t>()) {
            return false;
        }
        const std::int64_t index = entry.first.as<std::int64_t>();
        if (index <= 0 || static_cast<std::size_t>(index) > length) {
            return false;
        }
        ++itemCount;
    }
    if (!packed && itemCount != length) {
        return false;
    }
    values.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        values.push_back(table.raw_get<sol::object>(index));
    }
    return true;
}

sol::object evaluateRuntimeExpression(sol::state_view lua,
                                      const sol::object& value,
                                      const sol::object& rawEnvironment) {
    if (!value.is<std::string>()) {
        return value;
    }
    const std::string expression = value.as<std::string>();
    sol::load_result loaded =
        lua.load("return " + expression, "=(data)", sol::load_mode::text);
    if (!loaded.valid()) {
        return value;
    }
    sol::protected_function function = loaded;
    sol::environment environment(lua, sol::create, lua.globals());
    if (rawEnvironment.is<sol::table>()) {
        for (const auto& entry : rawEnvironment.as<sol::table>()) {
            environment.raw_set(entry.first, entry.second);
        }
    }
    sol::set_environment(environment, function);
    sol::protected_function_result result = function();
    if (!result.valid()) {
        return value;
    }
    sol::object evaluated =
        result.return_count() == 0 ? nilObject(lua) : result.get<sol::object>();
    static const std::regex bareIdentifier(R"(^\s*[A-Za-z_][A-Za-z0-9_]*\s*$)");
    static const std::regex nilLiteral(R"(^\s*nil\s*$)");
    if (evaluated.get_type() == sol::type::lua_nil &&
        std::regex_match(expression, bareIdentifier) &&
        !std::regex_match(expression, nilLiteral)) {
        return value;
    }
    return evaluated;
}

sol::object runtimeIndex(sol::state_view lua, const sol::object& target,
                         const sol::object& key, bool raw) {
    if (raw) {
        return ludork::standard::class_runtime::rawGetOwnField(lua, target,
                                                               key);
    }
    if (!raw) {
        return protectedIndex(lua, target, key);
    }
    return nilObject(lua);
}

void runtimeAssign(sol::state_view lua, const sol::object& target,
                   const sol::object& key, const sol::object& value, bool raw) {
    if (raw && !target.is<sol::table>()) {
        throw std::invalid_argument("Raw assignment requires a table");
    }
    if (!raw) {
        protectedAssign(lua, target, key, value);
        return;
    }
    lua_State* state = lua.lua_state();
    target.push();
    key.push();
    value.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

std::vector<sol::object> runtimeKeys(sol::state_view lua,
                                     const sol::object& target, bool raw) {
    std::vector<sol::object> keys;
    if (raw) {
        const sol::table ownKeys =
            ludork::standard::class_runtime::getOwnKeys(lua, target);
        keys.reserve(ownKeys.size());
        for (std::size_t index = 1; index <= ownKeys.size(); ++index) {
            keys.push_back(ownKeys.raw_get<sol::object>(index));
        }
        return keys;
    }

    const sol::object rawPairs = lua.globals().raw_get<sol::object>("pairs");
    if (!rawPairs.is<sol::protected_function>()) {
        throw std::runtime_error("Lua pairs function is not defined");
    }
    sol::protected_function pairs = rawPairs.as<sol::protected_function>();
    sol::protected_function_result initialized = pairs(target);
    if (!initialized.valid()) {
        const sol::error error = initialized;
        throw std::runtime_error(error.what());
    }
    if (initialized.return_count() < 3) {
        return keys;
    }
    const sol::object rawIterator = initialized.get<sol::object>(0);
    if (!rawIterator.is<sol::protected_function>()) {
        return keys;
    }
    sol::protected_function iterator =
        rawIterator.as<sol::protected_function>();
    const sol::object iteratorState = initialized.get<sol::object>(1);
    sol::object control = initialized.get<sol::object>(2);
    for (;;) {
        sol::protected_function_result next = iterator(iteratorState, control);
        if (!next.valid()) {
            const sol::error error = next;
            throw std::runtime_error(error.what());
        }
        if (next.return_count() == 0) {
            break;
        }
        sol::object key = next.get<sol::object>(0);
        if (!key.valid() || key.get_type() == sol::type::lua_nil) {
            break;
        }
        keys.push_back(key);
        control = key;
    }
    return keys;
}

sol::table runtimeStringKeys(sol::state_view lua,
                             const std::vector<sol::object>& keys) {
    sol::table result = lua.create_table();
    std::size_t index = 1;
    for (const sol::object& key : keys) {
        if (key.is<std::string>()) {
            result.raw_set(index++, key);
        }
    }
    return result;
}

sol::object runtimeTypeMetadata(sol::state_view lua,
                                const sol::table& classType);

std::vector<std::string> debugRuntimeMethodParameterNames(
    sol::state_view lua, const sol::object& method) {
    std::vector<std::string> names;
    if (!method.is<sol::protected_function>()) {
        return names;
    }
    const sol::object rawDebug = lua.globals().raw_get<sol::object>("debug");
    if (!rawDebug.is<sol::table>()) {
        return names;
    }
    const sol::table debug = rawDebug.as<sol::table>();
    const sol::object rawGetInfo = debug.raw_get<sol::object>("getinfo");
    const sol::object rawGetLocal = debug.raw_get<sol::object>("getlocal");
    if (!rawGetInfo.is<sol::protected_function>() ||
        !rawGetLocal.is<sol::protected_function>()) {
        return names;
    }
    sol::protected_function getInfo = rawGetInfo.as<sol::protected_function>();
    sol::protected_function_result infoResult = getInfo(method, "u");
    const sol::object rawInfo = checkedResult(lua, infoResult);
    if (!rawInfo.is<sol::table>()) {
        return names;
    }
    const sol::object rawCount =
        rawInfo.as<sol::table>().raw_get<sol::object>("nparams");
    const std::size_t count =
        rawCount.is<std::size_t>() ? rawCount.as<std::size_t>() : 0;
    sol::protected_function getLocal =
        rawGetLocal.as<sol::protected_function>();
    names.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        sol::protected_function_result localResult = getLocal(method, index);
        if (!localResult.valid()) {
            const sol::error error = localResult;
            throw std::runtime_error(error.what());
        }
        if (localResult.return_count() == 0) {
            continue;
        }
        const sol::object rawName = localResult.get<sol::object>(0);
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (name != "self") {
            names.push_back(name);
        }
    }
    return names;
}

std::optional<std::vector<std::string>> runtimeEventParameterNames(
    sol::state_view lua, const sol::table& classType,
    const std::string& eventName) {
    for (const sol::table& current : runtimeClassMro(lua, classType)) {
        const sol::object rawMetadata = runtimeTypeMetadata(lua, current);
        if (!rawMetadata.is<sol::table>()) {
            continue;
        }
        const sol::object rawEvent =
            rawMetadata.as<sol::table>().raw_get<sol::object>(eventName);
        if (!rawEvent.is<sol::table>()) {
            continue;
        }
        const sol::object rawParameters =
            rawEvent.as<sol::table>().raw_get<sol::object>("parameters");
        std::vector<std::string> names;
        if (!rawParameters.is<sol::table>()) {
            return names;
        }
        const sol::table parameters = rawParameters.as<sol::table>();
        names.reserve(parameters.size());
        for (std::size_t index = 1; index <= parameters.size(); ++index) {
            const sol::object rawName = parameters.raw_get<sol::object>(index);
            if (rawName.is<std::string>()) {
                names.push_back(rawName.as<std::string>());
            }
        }
        return names;
    }
    return std::nullopt;
}

std::vector<std::string> runtimeMethodParameterNames(
    sol::state_view lua, const sol::object& method, const sol::table& classType,
    const std::string& eventName) {
    const std::optional<std::vector<std::string>> metadataNames =
        runtimeEventParameterNames(lua, classType, eventName);
    return metadataNames.has_value()
               ? *metadataNames
               : debugRuntimeMethodParameterNames(lua, method);
}

void invokeNamedRuntimeMethod(sol::state_view lua, const sol::object& object,
                              const sol::object& method,
                              const sol::table& classType,
                              const std::string& eventName,
                              const sol::object& rawKeywordArguments) {
    if (!method.is<sol::protected_function>()) {
        return;
    }
    const sol::table keywordArguments =
        rawKeywordArguments.is<sol::table>()
            ? rawKeywordArguments.as<sol::table>()
            : lua.create_table();
    const std::vector<std::string> names =
        runtimeMethodParameterNames(lua, method, classType, eventName);
    std::unordered_set<std::string> accepted(names.begin(), names.end());
    for (const sol::object& key :
         runtimeKeys(lua, sol::make_object(lua, keywordArguments), false)) {
        if (key.is<std::string>() &&
            !accepted.contains(key.as<std::string>())) {
            throw std::invalid_argument(
                "Unexpected blueprint event argument '" +
                key.as<std::string>() + "'");
        }
    }
    std::vector<sol::object> arguments;
    arguments.reserve(names.size() + 1);
    arguments.push_back(object);
    for (const std::string& name : names) {
        arguments.push_back(keywordArguments.get<sol::object>(name));
    }
    sol::protected_function function = method.as<sol::protected_function>();
    sol::protected_function_result result = function(sol::as_args(arguments));
    checkedResult(lua, result);
}

bool calculateRuntimeMethodHasImplementation(sol::state_view lua,
                                             const sol::object& method) {
    if (!method.is<sol::protected_function>()) {
        return false;
    }
    const sol::object rawDebug = lua.globals().raw_get<sol::object>("debug");
    if (!rawDebug.is<sol::table>()) {
        return true;
    }
    const sol::object rawGetInfo =
        rawDebug.as<sol::table>().raw_get<sol::object>("getinfo");
    if (!rawGetInfo.is<sol::protected_function>()) {
        return true;
    }
    sol::protected_function getInfo = rawGetInfo.as<sol::protected_function>();
    sol::protected_function_result infoResult = getInfo(method, "S");
    const sol::object rawInfo = checkedResult(lua, infoResult);
    if (!rawInfo.is<sol::table>()) {
        return true;
    }
    const sol::table info = rawInfo.as<sol::table>();
    const sol::object rawWhat = info.raw_get<sol::object>("what");
    const sol::object rawSource = info.raw_get<sol::object>("source");
    if (!rawWhat.is<std::string>() || rawWhat.as<std::string>() != "Lua" ||
        !rawSource.is<std::string>()) {
        return true;
    }
    const std::string source = rawSource.as<std::string>();
    if (source.empty() || source.front() != '@') {
        return true;
    }
    const sol::object rawFirst = info.raw_get<sol::object>("linedefined");
    const sol::object rawLast = info.raw_get<sol::object>("lastlinedefined");
    if (!rawFirst.is<std::size_t>() || !rawLast.is<std::size_t>()) {
        return true;
    }
    std::ifstream file(ludork::standard::pathFromUtf8(source.substr(1)));
    if (!file) {
        return true;
    }
    const std::size_t firstLine = rawFirst.as<std::size_t>();
    const std::size_t lastLine = rawLast.as<std::size_t>();
    std::string text;
    std::string line;
    std::size_t lineIndex = 0;
    while (std::getline(file, line)) {
        ++lineIndex;
        if (lineIndex >= firstLine && lineIndex <= lastLine) {
            const std::size_t comment = line.find("--");
            if (comment != std::string::npos) {
                line.erase(comment);
            }
            text += line;
            text.push_back('\n');
        }
        if (lineIndex > lastLine) {
            break;
        }
    }
    text = std::regex_replace(
        text, std::regex(R"(^\s*(?:local\s+)?function[^\n\(]*\([^\)]*\)\s*)"),
        std::string());
    text = std::regex_replace(text, std::regex(R"(\s*end\s*$)"), std::string());
    text = std::regex_replace(text, std::regex(R"(\s+)"), std::string(" "));
    const std::size_t start = text.find_first_not_of(' ');
    if (start == std::string::npos) {
        return false;
    }
    const std::size_t end = text.find_last_not_of(' ');
    const std::string body = text.substr(start, end - start + 1);
    return !body.empty() && body != "return" && body != "return nil";
}

bool runtimeMethodHasImplementation(sol::state_view lua,
                                    const sol::object& method) {
    if (!method.is<sol::protected_function>()) {
        return false;
    }
    sol::table cache =
        registryTable(lua, BLUEPRINT_IMPLEMENTATION_CACHE_KEY, "k");
    const sol::object cached = cache.raw_get<sol::object>(method);
    if (cached.is<bool>()) {
        return cached.as<bool>();
    }
    const bool result = calculateRuntimeMethodHasImplementation(lua, method);
    cache.raw_set(method, result);
    return result;
}

sol::object blueprintEngineType(sol::state_view lua, const char* name) {
    return requireRuntimeType(lua, "Engine", name);
}

bool blueprintIsInstance(sol::this_state state, const sol::object& value,
                         const sol::object& type) {
    return type.is<sol::table>() &&
           isInstance(state, value, type.as<sol::table>());
}

sol::object callRuntimeMethodFirst(
    sol::state_view lua, const sol::object& object, const char* name,
    const std::vector<sol::object>& arguments = {}) {
    const sol::object method =
        runtimeIndex(lua, object, sol::make_object(lua, name), false);
    if (!method.is<sol::protected_function>()) {
        return nilObject(lua);
    }
    std::vector<sol::object> values;
    values.reserve(arguments.size() + 1);
    values.push_back(object);
    values.insert(values.end(), arguments.begin(), arguments.end());
    sol::protected_function function = method.as<sol::protected_function>();
    sol::protected_function_result result = function(sol::as_args(values));
    return checkedResult(lua, result);
}

std::optional<double> runtimeNumber(sol::state_view lua,
                                    const sol::object& value) {
    const sol::object rawToNumber =
        lua.globals().raw_get<sol::object>("tonumber");
    if (!rawToNumber.is<sol::protected_function>()) {
        return std::nullopt;
    }
    sol::protected_function toNumber =
        rawToNumber.as<sol::protected_function>();
    sol::protected_function_result converted = toNumber(value);
    const sol::object result = checkedResult(lua, converted);
    return result.is<double>() ? std::optional<double>(result.as<double>())
                               : std::nullopt;
}

bool blueprintGraphHasExecutableEvent(sol::state_view lua,
                                      const sol::object& graph,
                                      const std::string& eventName) {
    if (!graph.valid() || graph.get_type() == sol::type::lua_nil) {
        return false;
    }
    const sol::object direct = runtimeIndex(
        lua, graph, sol::make_object(lua, "hasExecutableEvent"), false);
    if (direct.is<sol::protected_function>()) {
        return luaBoolean(
            callRuntimeMethodFirst(lua, graph, "hasExecutableEvent",
                                   {sol::make_object(lua, eventName)}));
    }
    const sol::object startNodes =
        runtimeIndex(lua, graph, sol::make_object(lua, "startNodes"), false);
    const sol::object nodes =
        runtimeIndex(lua, graph, sol::make_object(lua, "nodes"), false);
    if (!startNodes.is<sol::table>() || !nodes.is<sol::table>()) {
        return false;
    }
    const sol::object rawStart =
        startNodes.as<sol::table>().get<sol::object>(eventName);
    const sol::object rawEventNodes =
        nodes.as<sol::table>().get<sol::object>(eventName);
    if (rawStart.get_type() == sol::type::lua_nil ||
        !rawEventNodes.is<sol::table>()) {
        return false;
    }
    const std::optional<double> start = runtimeNumber(lua, rawStart);
    return start.has_value() && *start >= 0.0 &&
           *start < static_cast<double>(rawEventNodes.as<sol::table>().size());
}

bool blueprintGraphDataHasExecutableEvent(sol::state_view lua,
                                          const sol::object& graphData,
                                          const std::string& eventName) {
    if (!graphData.is<sol::table>()) {
        return false;
    }
    const sol::table data = graphData.as<sol::table>();
    const sol::object nodeGraph = data.get<sol::object>("nodeGraph");
    const sol::object startNodes = data.get<sol::object>("startNodes");
    if (!nodeGraph.is<sol::table>() || !startNodes.is<sol::table>()) {
        return false;
    }
    const sol::object eventGraph =
        nodeGraph.as<sol::table>().get<sol::object>(eventName);
    const sol::object rawStart =
        startNodes.as<sol::table>().get<sol::object>(eventName);
    if (!eventGraph.is<sol::table>() ||
        rawStart.get_type() == sol::type::lua_nil) {
        return false;
    }
    const sol::object nodes =
        eventGraph.as<sol::table>().get<sol::object>("nodes");
    const std::optional<double> start = runtimeNumber(lua, rawStart);
    return nodes.is<sol::table>() && start.has_value() && *start >= 0.0 &&
           *start < static_cast<double>(nodes.as<sol::table>().size());
}

bool generatedBlueprintGraphHasExecutableEvent(sol::state_view lua,
                                               const sol::table& classType,
                                               const std::string& eventName) {
    const sol::object rawScriptMixin =
        classType.get<sol::object>("scriptMixin");
    if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
        return false;
    }
    const sol::object rawPath =
        classType.raw_get<sol::object>("__blueprintClassPath");
    if (!rawPath.is<std::string>()) {
        return false;
    }
    const sol::table result = callRegisteredRuntimeService(
        lua, "nodegraph.classGraphHasExecutableEvent",
        {rawPath, sol::make_object(lua, eventName)});
    return luaBoolean(runtimeResolverArgument(lua, result, 1));
}

sol::object generatedBlueprintGraph(sol::state_view lua,
                                    const sol::object& object,
                                    const sol::table& classType) {
    sol::object rawCache = runtimeIndex(
        lua, object, sol::make_object(lua, "_parentGraphs"), false);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        runtimeAssign(lua, object, sol::make_object(lua, "_parentGraphs"),
                      sol::make_object(lua, cache), false);
    }
    sol::object graph = cache.raw_get<sol::object>(classType);
    if (graph.get_type() == sol::type::lua_nil) {
        const sol::object rawPath =
            classType.raw_get<sol::object>("__blueprintClassPath");
        if (!rawPath.is<std::string>()) {
            return nilObject(lua);
        }
        const sol::table result = callRegisteredRuntimeService(
            lua, "nodegraph.instantiateClassGraph", {rawPath, object});
        graph = runtimeResolverArgument(lua, result, 1);
        if (graph.valid() && graph.get_type() != sol::type::lua_nil) {
            cache.raw_set(classType, graph);
        }
    }
    return graph;
}

sol::table blueprintEventKeywordArguments(
    sol::state_view lua, const sol::table& classType,
    const std::string& eventName, const sol::object& rawArguments,
    const sol::object& rawKeywordArguments) {
    sol::table result = lua.create_table();
    if (rawKeywordArguments.is<sol::table>()) {
        for (const auto& entry : rawKeywordArguments.as<sol::table>()) {
            result.raw_set(entry.first, entry.second);
        }
    }
    if (!rawArguments.is<sol::table>() ||
        rawArguments.as<sol::table>().size() == 0) {
        return result;
    }
    const sol::object method = classType.get<sol::object>(eventName);
    if (!method.is<sol::protected_function>()) {
        return result;
    }
    const std::vector<std::string> names =
        runtimeMethodParameterNames(lua, method, classType, eventName);
    const sol::table arguments = rawArguments.as<sol::table>();
    const std::size_t count = std::min(arguments.size(), names.size());
    for (std::size_t index = 1; index <= count; ++index) {
        if (result.get<sol::object>(names[index - 1]).get_type() ==
            sol::type::lua_nil) {
            result.set(names[index - 1], arguments.get<sol::object>(index));
        }
    }
    return result;
}

void mergeBlueprintLocalArguments(sol::state_view lua,
                                  const sol::table& classType,
                                  const std::string& eventName,
                                  sol::table keywordArguments,
                                  const sol::object& localGraph) {
    if (!localGraph.is<sol::table>()) {
        return;
    }
    const sol::object method = classType.get<sol::object>(eventName);
    if (!method.is<sol::protected_function>()) {
        return;
    }
    for (const std::string& name :
         runtimeMethodParameterNames(lua, method, classType, eventName)) {
        if (keywordArguments.get<sol::object>(name).get_type() !=
            sol::type::lua_nil) {
            continue;
        }
        const sol::object value =
            localGraph.as<sol::table>().get<sol::object>("__" + name + "__");
        if (value.get_type() != sol::type::lua_nil) {
            keywordArguments.set(name, value);
        }
    }
}

bool executeBlueprintGraph(sol::state_view lua, const sol::object& graph,
                           const std::string& eventName,
                           const sol::object& rawKeywordArguments,
                           const sol::object& localGraph,
                           const std::function<void()>& onComplete) {
    const std::shared_ptr<Graph> nativeGraph =
        graph.as<std::shared_ptr<Graph>>();
    if (!nativeGraph->tryLockExecution(eventName)) {
        return false;
    }
    if (onComplete) {
        nativeGraph->addExecutionCompleteCallback(eventName, onComplete);
    }
    const RuntimeIdentityPtr oldLocalGraph = nativeGraph->getLocalGraph();
    RuntimeValue context;
    RuntimeValue oldContextGraph;
    std::vector<std::pair<std::string, RuntimeValue>> oldEventParameters;
    bool contextGraphSet = false;

    const auto restore = [&]() noexcept {
        for (const auto& [name, value] : oldEventParameters) {
            try {
                resolveRuntime("nodegraph.context",
                               {context, RuntimeValue(std::string("set")),
                                RuntimeValue(name), value});
            } catch (...) {}
        }
        if (contextGraphSet) {
            try {
                resolveRuntime(
                    "nodegraph.context",
                    {context, RuntimeValue(std::string("set")),
                     RuntimeValue(std::string("__graph__")), oldContextGraph});
            } catch (...) {}
        }
        nativeGraph->setLocalGraph(oldLocalGraph);
        nativeGraph->completeExecution(eventName);
    };
    try {
        if (localGraph.valid() && localGraph.get_type() != sol::type::lua_nil) {
            nativeGraph->setLocalGraph(
                ludork_core::readLuaValue<RuntimeIdentityPtr>(localGraph));
        }
        RuntimeIdentityPtr activeLocalGraph = nativeGraph->getLocalGraph();
        if (activeLocalGraph == nullptr) {
            const std::vector<RuntimeValue> created = resolveRuntime(
                "nodegraph.context",
                {nativeGraph->parentClass, nativeGraph->getParent()});
            if (!created.empty()) {
                if (const RuntimeIdentityPtr* identity =
                        created.front().getIf<RuntimeIdentityPtr>()) {
                    activeLocalGraph = *identity;
                }
            }
            nativeGraph->setLocalGraph(activeLocalGraph);
        }
        if (activeLocalGraph == nullptr) {
            throw std::runtime_error("Blueprint graph has no local context");
        }

        context = RuntimeValue(activeLocalGraph);
        const std::vector<RuntimeValue> oldGraph = resolveRuntime(
            "nodegraph.context", {context, RuntimeValue(std::string("get")),
                                  RuntimeValue(std::string("__graph__"))});
        oldContextGraph = oldGraph.empty() ? RuntimeValue() : oldGraph.front();
        resolveRuntime("nodegraph.context",
                       {context, RuntimeValue(std::string("set")),
                        RuntimeValue(std::string("__graph__")),
                        nativeGraph->getGraphContext()});
        contextGraphSet = true;
        if (rawKeywordArguments.is<sol::table>()) {
            for (const auto& entry : rawKeywordArguments.as<sol::table>()) {
                if (!entry.first.is<std::string>()) {
                    continue;
                }
                const std::string name =
                    "__" + entry.first.as<std::string>() + "__";
                const std::vector<RuntimeValue> oldValue =
                    resolveRuntime("nodegraph.context",
                                   {context, RuntimeValue(std::string("get")),
                                    RuntimeValue(name)});
                oldEventParameters.emplace_back(
                    name, oldValue.empty() ? RuntimeValue() : oldValue.front());
                resolveRuntime(
                    "nodegraph.context",
                    {context, RuntimeValue(std::string("set")),
                     RuntimeValue(name),
                     ludork_core::readLuaValue<RuntimeValue>(entry.second)});
            }
        }
        nativeGraph->execute(eventName);
    } catch (...) {
        restore();
        throw;
    }
    restore();
    return true;
}

bool tryExecuteInfoBlueprintGraph(sol::this_state state,
                                  const sol::object& object,
                                  const std::string& eventName,
                                  const sol::object& keywordArguments,
                                  const std::function<void()>& onComplete) {
    sol::state_view lua(state);
    const sol::object infoBase = blueprintEngineType(lua, "InfoBase");
    if (!blueprintIsInstance(state, object, infoBase)) {
        return false;
    }
    const sol::object graph =
        callRuntimeMethodFirst(lua, object, "getInfoGraph");
    if (!blueprintGraphHasExecutableEvent(lua, graph, eventName)) {
        return false;
    }
    if (!executeBlueprintGraph(lua, graph, eventName, keywordArguments,
                               nilObject(lua), onComplete)) {
        invokeCompletion(onComplete);
    }
    return true;
}

bool classHasBlueprintEvent(sol::this_state state, const sol::object& rawClass,
                            const std::string& eventName) {
    sol::state_view lua(state);
    if (!rawClass.is<sol::table>()) {
        return false;
    }
    const sol::table classType = rawClass.as<sol::table>();
    if (!isClass(classType) && !isNativeType(lua, classType)) {
        return false;
    }
    const sol::object generated =
        classType.raw_get<sol::object>("_GENERATED_CLASS");
    if (luaBoolean(generated)) {
        const sol::object rawScriptMixin =
            classType.get<sol::object>("scriptMixin");
        if (rawScriptMixin.is<bool>() && rawScriptMixin.as<bool>()) {
            const sol::object method =
                classType.raw_get<sol::object>(eventName);
            if (runtimeMethodHasImplementation(lua, method)) {
                return true;
            }
            return classHasBlueprintEvent(
                state, classType.raw_get<sol::object>("__base"), eventName);
        }
        if (generatedBlueprintGraphHasExecutableEvent(lua, classType,
                                                      eventName)) {
            return true;
        }
        return classHasBlueprintEvent(
            state, classType.raw_get<sol::object>("__base"), eventName);
    }
    const sol::object graph = classType.raw_get<sol::object>("_graph");
    if (blueprintGraphHasExecutableEvent(lua, graph, eventName)) {
        return true;
    }
    const sol::object method = classType.raw_get<sol::object>(eventName);
    if (isClass(classType) && runtimeMethodHasImplementation(lua, method)) {
        return true;
    }
    return classHasBlueprintEvent(
        state, classType.raw_get<sol::object>("__base"), eventName);
}

bool hasBlueprintEvent(sol::this_state state, const sol::object& object,
                       const std::string& eventName) {
    sol::state_view lua(state);
    if (!object.valid() || object.get_type() == sol::type::lua_nil ||
        eventName.empty()) {
        return false;
    }
    const sol::object rawClass = classType(state, object);
    const bool scriptMixin =
        rawClass.is<sol::table>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").is<bool>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").as<bool>();
    const sol::object actorType = blueprintEngineType(lua, "Actor");
    const sol::object actorGraph =
        !scriptMixin && blueprintIsInstance(state, object, actorType)
            ? callRuntimeMethodFirst(lua, object, "getGraph")
            : nilObject(lua);
    if (blueprintGraphHasExecutableEvent(lua, actorGraph, eventName)) {
        return true;
    }
    const sol::object infoBase = blueprintEngineType(lua, "InfoBase");
    const sol::object infoGraph =
        blueprintIsInstance(state, object, infoBase)
            ? callRuntimeMethodFirst(lua, object, "getInfoGraph")
            : nilObject(lua);
    if (blueprintGraphHasExecutableEvent(lua, infoGraph, eventName)) {
        return true;
    }
    sol::object instanceMethod = nilObject(lua);
    instanceMethod = ludork::standard::class_runtime::rawGetOwnField(
        lua, object, sol::make_object(lua, eventName));
    if (runtimeMethodHasImplementation(lua, instanceMethod)) {
        return true;
    }
    return classHasBlueprintEvent(state, rawClass, eventName);
}

bool executeParentBlueprintEvent(
    sol::this_state state, const sol::object& object,
    const sol::object& rawClass, const std::string& eventName,
    const sol::object& arguments, const sol::object& keywordArguments,
    const sol::object& localGraph, const std::function<void()>& onComplete) {
    sol::state_view lua(state);
    if (!rawClass.is<sol::table>()) {
        return false;
    }
    const sol::object rawParent =
        rawClass.as<sol::table>().raw_get<sol::object>("__base");
    if (!rawParent.is<sol::table>()) {
        return false;
    }
    const sol::table parent = rawParent.as<sol::table>();
    sol::table eventArguments = blueprintEventKeywordArguments(
        lua, parent, eventName, arguments, keywordArguments);
    mergeBlueprintLocalArguments(lua, parent, eventName, eventArguments,
                                 localGraph);
    if (luaBoolean(parent.raw_get<sol::object>("_GENERATED_CLASS"))) {
        if (generatedBlueprintGraphHasExecutableEvent(lua, parent, eventName)) {
            const sol::object graph =
                generatedBlueprintGraph(lua, object, parent);
            if (graph.valid() && graph.get_type() != sol::type::lua_nil) {
                if (!executeBlueprintGraph(
                        lua, graph, eventName,
                        sol::make_object(lua, eventArguments), localGraph,
                        onComplete)) {
                    invokeCompletion(onComplete);
                }
                return true;
            }
        }
        return executeParentBlueprintEvent(
            state, object, rawParent, eventName, nilObject(lua),
            sol::make_object(lua, eventArguments), localGraph, onComplete);
    }

    const sol::object graph = parent.get<sol::object>("_graph");
    if (graph.valid() && graph.get_type() != sol::type::lua_nil &&
        luaBoolean(callRuntimeMethodFirst(
            lua, graph, "hasKey", {sol::make_object(lua, eventName)}))) {
        const sol::object startNodes = runtimeIndex(
            lua, graph, sol::make_object(lua, "startNodes"), false);
        if (startNodes.is<sol::table>() &&
            startNodes.as<sol::table>()
                    .get<sol::object>(eventName)
                    .get_type() != sol::type::lua_nil) {
            if (!executeBlueprintGraph(lua, graph, eventName,
                                       sol::make_object(lua, eventArguments),
                                       localGraph, onComplete)) {
                invokeCompletion(onComplete);
            }
            return true;
        }
        return executeParentBlueprintEvent(
            state, object, rawParent, eventName, nilObject(lua),
            sol::make_object(lua, eventArguments), localGraph, onComplete);
    }

    const sol::object method = parent.get<sol::object>(eventName);
    if (!method.is<sol::protected_function>()) {
        return executeParentBlueprintEvent(
            state, object, rawParent, eventName, nilObject(lua),
            sol::make_object(lua, eventArguments), localGraph, onComplete);
    }
    invokeNamedRuntimeMethod(lua, object, method, parent, eventName,
                             sol::make_object(lua, eventArguments));
    invokeCompletion(onComplete);
    return true;
}

void dispatchBlueprintEvent(sol::this_state state, const sol::object& object,
                            const sol::object& rawObjectType,
                            const std::string& eventName,
                            const sol::object& rawKeywordArguments,
                            const std::function<void()>& onComplete) {
    sol::state_view lua(state);
    const sol::object isDestroyed =
        runtimeIndex(lua, object, sol::make_object(lua, "isDestroyed"), false);
    if (isDestroyed.is<sol::protected_function>() &&
        luaBoolean(callRuntimeMethodFirst(lua, object, "isDestroyed"))) {
        invokeCompletion(onComplete);
        return;
    }
    const sol::object objectType = rawObjectType.is<sol::table>()
                                       ? rawObjectType
                                       : classType(state, object);
    if (!blueprintIsInstance(state, object, objectType)) {
        invokeCompletion(onComplete);
        return;
    }
    const sol::table keywordArguments =
        rawKeywordArguments.is<sol::table>()
            ? rawKeywordArguments.as<sol::table>()
            : lua.create_table();
    const sol::object rawClass = classType(state, object);
    const bool scriptMixin =
        rawClass.is<sol::table>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").is<bool>() &&
        rawClass.as<sol::table>().get<sol::object>("scriptMixin").as<bool>();
    if (scriptMixin) {
        if (onComplete) {
            const std::shared_ptr<CompletionBarrier> barrier =
                std::make_shared<CompletionBarrier>(2, onComplete);
            const bool infoExecuted = tryExecuteInfoBlueprintGraph(
                state, object, eventName,
                sol::make_object(lua, keywordArguments), [barrier]() {
                    barrier->complete();
                });
            if (!infoExecuted) {
                barrier->complete();
            }
            const sol::object method = runtimeIndex(
                lua, object, sol::make_object(lua, eventName), false);
            invokeNamedRuntimeMethod(lua, object, method,
                                     rawClass.as<sol::table>(), eventName,
                                     sol::make_object(lua, keywordArguments));
            barrier->complete();
            return;
        }
        tryExecuteInfoBlueprintGraph(state, object, eventName,
                                     sol::make_object(lua, keywordArguments),
                                     {});
        const sol::object method =
            runtimeIndex(lua, object, sol::make_object(lua, eventName), false);
        invokeNamedRuntimeMethod(lua, object, method, rawClass.as<sol::table>(),
                                 eventName,
                                 sol::make_object(lua, keywordArguments));
        return;
    }
    const sol::object actorType = blueprintEngineType(lua, "Actor");
    const sol::object graph =
        blueprintIsInstance(state, object, actorType)
            ? callRuntimeMethodFirst(lua, object, "getGraph")
            : nilObject(lua);
    const bool generated =
        rawClass.is<sol::table>() &&
        luaBoolean(
            rawClass.as<sol::table>().raw_get<sol::object>("_GENERATED_CLASS"));
    if (generated && graph.valid() && graph.get_type() != sol::type::lua_nil) {
        if (luaBoolean(callRuntimeMethodFirst(
                lua, graph, "hasKey", {sol::make_object(lua, eventName)}))) {
            const sol::object startNodes = runtimeIndex(
                lua, graph, sol::make_object(lua, "startNodes"), false);
            if (startNodes.is<sol::table>() &&
                startNodes.as<sol::table>()
                        .get<sol::object>(eventName)
                        .get_type() != sol::type::lua_nil) {
                if (!executeBlueprintGraph(
                        lua, graph, eventName,
                        sol::make_object(lua, keywordArguments), nilObject(lua),
                        onComplete)) {
                    invokeCompletion(onComplete);
                }
                return;
            }
            if (tryExecuteInfoBlueprintGraph(
                    state, object, eventName,
                    sol::make_object(lua, keywordArguments), onComplete)) {
                return;
            }
        }
        if (tryExecuteInfoBlueprintGraph(
                state, object, eventName,
                sol::make_object(lua, keywordArguments), onComplete)) {
            return;
        }
        if (executeParentBlueprintEvent(state, object, rawClass, eventName,
                                        nilObject(lua),
                                        sol::make_object(lua, keywordArguments),
                                        nilObject(lua), onComplete)) {
            return;
        }
        const sol::object method =
            runtimeIndex(lua, object, sol::make_object(lua, eventName), false);
        invokeNamedRuntimeMethod(lua, object, method, rawClass.as<sol::table>(),
                                 eventName,
                                 sol::make_object(lua, keywordArguments));
        invokeCompletion(onComplete);
        return;
    }
    if (tryExecuteInfoBlueprintGraph(state, object, eventName,
                                     sol::make_object(lua, keywordArguments),
                                     onComplete)) {
        return;
    }
    const sol::object method =
        runtimeIndex(lua, object, sol::make_object(lua, eventName), false);
    invokeNamedRuntimeMethod(lua, object, method, rawClass.as<sol::table>(),
                             eventName,
                             sol::make_object(lua, keywordArguments));
    invokeCompletion(onComplete);
}

std::string trimRuntimeString(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return std::string();
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

sol::object resolveGeneralDataDictionary(sol::state_view lua,
                                         const sol::object& value) {
    if (value.is<sol::table>()) {
        sol::table result = lua.create_table();
        for (const auto& entry : value.as<sol::table>()) {
            sol::object item = entry.second;
            if (item.is<std::string>()) {
                const std::string text = item.as<std::string>();
                item =
                    trimRuntimeString(text).empty()
                        ? sol::make_object(lua, std::string())
                        : evaluateRuntimeExpression(lua, item, nilObject(lua));
            }
            result.raw_set(entry.first, item);
        }
        return sol::make_object(lua, result);
    }
    if (value.is<std::string>()) {
        const sol::object evaluated =
            evaluateRuntimeExpression(lua, value, nilObject(lua));
        if (evaluated.is<sol::table>()) {
            return resolveGeneralDataDictionary(lua, evaluated);
        }
    }
    return sol::make_object(lua, lua.create_table());
}

bool setBlueprintComponentField(sol::state_view lua, const sol::object& object,
                                const std::string& name,
                                const sol::object& value) {
    static_cast<void>(lua);
    return ludork::engine::components::setComponentFieldValue(
        ludork_core::readLuaValue<RuntimeValue>(object), name,
        ludork_core::readLuaValue<RuntimeValue>(value));
}

void applyBlueprintGeneralData(sol::state_view lua, const sol::object& object,
                               const sol::object& rawData,
                               const sol::object& rawParameterTypes) {
    if (!rawData.is<sol::table>()) {
        return;
    }
    const sol::table parameterTypes = rawParameterTypes.is<sol::table>()
                                          ? rawParameterTypes.as<sol::table>()
                                          : lua.create_table();
    for (const auto& entry : rawData.as<sol::table>()) {
        if (!entry.first.is<std::string>()) {
            continue;
        }
        const std::string name = entry.first.as<std::string>();
        if (name.empty() || name.front() == '_') {
            continue;
        }
        sol::object value = entry.second;
        const sol::object rawParameter = parameterTypes.get<sol::object>(name);
        if (rawParameter.is<sol::table>()) {
            const sol::object rawType =
                rawParameter.as<sol::table>().get<sol::object>("type");
            const std::string type = rawType.is<std::string>()
                                         ? rawType.as<std::string>()
                                         : std::string();
            if (type == "dict") {
                value = resolveGeneralDataDictionary(lua, value);
            } else if (type == "string") {
                const RuntimeValue rawValue =
                    ludork_core::readLuaValue<RuntimeValue>(value);
                const std::vector<RuntimeValue> resolved =
                    resolveRuntime("blueprint.resolveStringValue", {rawValue});
                if (!resolved.empty()) {
                    value = ludork_core::writeLuaValue(lua, resolved.front());
                }
            } else if (type != "int" && type != "float" && type != "bool" &&
                       type != "list" &&
                       !std::regex_match(type,
                                         std::regex(R"(^tuple\[\d+\]$)"))) {
                value = evaluateRuntimeExpression(lua, value, nilObject(lua));
            }
        } else {
            value = evaluateRuntimeExpression(lua, value, nilObject(lua));
        }
        if (!setBlueprintComponentField(lua, object, name, value)) {
            runtimeAssign(lua, object, sol::make_object(lua, name), value,
                          false);
        }
    }
}

void initializeBlueprintInfo(sol::this_state state, const sol::object& object,
                             const sol::object& dataProvider) {
    sol::state_view lua(state);
    const sol::object rawClass = classType(state, object);
    if (!rawClass.is<sol::table>()) {
        return;
    }
    const sol::object rawInfoType =
        runtimeIndex(lua, rawClass, sol::make_object(lua, "_infoType"), false);
    if (!rawInfoType.is<std::string>() ||
        rawInfoType.as<std::string>().empty()) {
        return;
    }
    const sol::object rawGetGeneralData = runtimeIndex(
        lua, dataProvider, sol::make_object(lua, "getGeneralData"), false);
    if (!rawGetGeneralData.is<sol::protected_function>()) {
        return;
    }
    sol::protected_function getGeneralData =
        rawGetGeneralData.as<sol::protected_function>();
    sol::protected_function_result loaded =
        getGeneralData(rawInfoType.as<std::string>());
    sol::object rawData = checkedResult(lua, loaded);
    if (!rawData.is<sol::table>()) {
        rawData = sol::make_object(lua, lua.create_table());
    }
    const sol::table data = rawData.as<sol::table>();
    const sol::object rawMembers = data.get<sol::object>("members");
    const sol::object rawId =
        runtimeIndex(lua, object, sol::make_object(lua, "ID"), false);
    if (!rawMembers.is<sol::table>() || !rawId.is<std::string>()) {
        return;
    }
    const sol::object member =
        rawMembers.as<sol::table>().get<sol::object>(rawId.as<std::string>());
    if (!member.is<sol::table>()) {
        return;
    }
    sol::object parameters = data.get<sol::object>("params");
    if (!parameters.is<sol::table>()) {
        parameters = sol::make_object(lua, lua.create_table());
    }
    applyBlueprintGeneralData(lua, object, member, parameters);
    const sol::object graphData =
        member.as<sol::table>().get<sol::object>("_graph");
    const sol::object rawGenerator = runtimeIndex(
        lua, dataProvider, sol::make_object(lua, "genGraphFromData"), false);
    if (graphData.get_type() == sol::type::lua_nil ||
        !rawGenerator.is<sol::protected_function>()) {
        return;
    }
    sol::protected_function generator =
        rawGenerator.as<sol::protected_function>();
    sol::protected_function_result generated =
        generator(graphData, object, rawClass);
    const sol::object graph = checkedResult(lua, generated);
    callRuntimeMethodFirst(lua, object, "setInfoGraph", {graph});
}

sol::object runtimeTypeMetadata(sol::state_view lua,
                                const sol::table& classType) {
    const sol::object native =
        classType.raw_get<sol::object>("__runtimeMetadata");
    if (native.is<sol::table>()) {
        return native;
    }
    std::optional<RuntimeClassIdentity> identity =
        resolveRuntimeClassIdentity(lua, sol::make_object(lua, classType));
    if (!identity.has_value() || identity->module.empty()) {
        return nilObject(lua);
    }
    const std::string metadataModule = identity->module + "_meta";
    const sol::object rawPackage =
        lua.globals().raw_get<sol::object>("package");
    if (!rawPackage.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::object rawSearchPath =
        rawPackage.as<sol::table>().raw_get<sol::object>("searchpath");
    const sol::object rawPath =
        rawPackage.as<sol::table>().raw_get<sol::object>("path");
    if (!rawSearchPath.is<sol::protected_function>() ||
        !rawPath.is<std::string>()) {
        return nilObject(lua);
    }
    sol::protected_function searchPath =
        rawSearchPath.as<sol::protected_function>();
    sol::protected_function_result searched =
        searchPath(metadataModule, rawPath);
    const sol::object found = checkedResult(lua, searched);
    if (!found.is<std::string>()) {
        return nilObject(lua);
    }
    const sol::table metadata = requireLuaTable(lua, metadataModule.c_str());
    std::string typeName = identity->type;
    if (typeName.empty() && identity->direct) {
        for (const auto& entry : metadata) {
            if (!entry.first.is<std::string>() ||
                !entry.second.is<sol::table>()) {
                continue;
            }
            if (!typeName.empty()) {
                throw std::runtime_error(
                    "Metadata module for directly returned class must contain "
                    "one "
                    "type: " +
                    metadataModule);
            }
            typeName = entry.first.as<std::string>();
        }
        if (typeName.empty()) {
            throw std::runtime_error(
                "Metadata module for directly returned class contains no "
                "type: " +
                metadataModule);
        }
        identity->descriptor.raw_set("type", typeName);
    }
    return typeName.empty() ? nilObject(lua)
                            : metadata.raw_get<sol::object>(typeName);
}

sol::table registeredBlueprintEvents(sol::state_view lua,
                                     const sol::object& rawClass) {
    sol::object target = rawClass;
    if (!target.is<sol::table>()) {
        target = blueprintEngineType(lua, "InfoBase");
    }
    sol::table result = lua.create_table();
    if (!target.is<sol::table>()) {
        return result;
    }
    std::unordered_set<std::string> included;
    std::vector<std::string> events;
    for (const sol::table& current :
         runtimeClassMro(lua, target.as<sol::table>())) {
        const sol::object rawMetadata = runtimeTypeMetadata(lua, current);
        if (!rawMetadata.is<sol::table>()) {
            continue;
        }
        for (const auto& entry : rawMetadata.as<sol::table>()) {
            if (!entry.first.is<std::string>() ||
                !entry.second.is<sol::table>()) {
                continue;
            }
            const sol::object rawType =
                entry.second.as<sol::table>().raw_get<sol::object>("type");
            if (!rawType.is<std::string>() ||
                rawType.as<std::string>() != "event") {
                continue;
            }
            const std::string name = entry.first.as<std::string>();
            if (included.insert(name).second) {
                events.push_back(name);
            }
        }
    }
    std::sort(events.begin(), events.end());
    std::size_t index = 1;
    for (const std::string& event : events) {
        result.raw_set(index++, event);
    }
    return result;
}

bool isRuntimeCallable(sol::state_view lua, const sol::object& value) {
    if (value.is<sol::function>()) {
        return true;
    }
    if (value.get_type() != sol::type::table &&
        value.get_type() != sol::type::userdata) {
        return false;
    }
    const sol::table metatable = objectMetatable(lua, value);
    return metatable.raw_get<sol::object>("__call").is<sol::function>();
}

sol::table packedRuntimeObjects(sol::state_view lua,
                                const std::vector<sol::object>& values) {
    sol::table result = lua.create_table(static_cast<int>(values.size()), 1);
    result.raw_set("n", values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index].valid() &&
            values[index].get_type() != sol::type::lua_nil) {
            result.raw_set(index + 1, values[index]);
        }
    }
    return result;
}

std::vector<sol::object> callNodeGraphCallable(
    sol::state_view lua, const sol::object& callable, const sol::object& self,
    const sol::object& rawArguments) {
    if (!isRuntimeCallable(lua, callable)) {
        throw std::invalid_argument("Node graph value is not callable");
    }
    lua_State* state = lua.lua_state();
    const int base = lua_gettop(state);
    callable.push();
    int argumentCount = 0;
    if (self.valid() && self.get_type() != sol::type::lua_nil) {
        self.push();
        ++argumentCount;
    }
    if (rawArguments.is<sol::table>()) {
        const sol::table arguments = rawArguments.as<sol::table>();
        const sol::object rawCount = arguments.raw_get<sol::object>("n");
        const std::size_t count = rawCount.is<std::size_t>()
                                      ? rawCount.as<std::size_t>()
                                      : arguments.size();
        for (std::size_t index = 1; index <= count; ++index) {
            arguments.raw_get<sol::object>(index).push();
            ++argumentCount;
        }
    }
    const int status =
        ludork::standard::protectedLuaCall(state, argumentCount, LUA_MULTRET);
    if (status != LUA_OK) {
        const char* rawError = lua_tostring(state, -1);
        const std::string message =
            rawError == nullptr ? "Node graph callable failed" : rawError;
        lua_settop(state, base);
        throw std::runtime_error(message);
    }
    const int count = lua_gettop(state) - base;
    std::vector<sol::object> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int index = 1; index <= count; ++index) {
        result.push_back(sol::stack::get<sol::object>(state, base + index));
    }
    lua_settop(state, base);
    return result;
}

sol::table nodeGraphContext(sol::state_view lua, const sol::table& arguments) {
    const sol::object first = runtimeResolverArgument(lua, arguments, 1);
    const sol::object second = runtimeResolverArgument(lua, arguments, 2);
    const sol::object third = runtimeResolverArgument(lua, arguments, 3);
    if (first.is<sol::table>() && second.is<std::string>()) {
        sol::table context = first.as<sol::table>();
        const std::string operation = second.as<std::string>();
        if (operation == "get") {
            const sol::object value =
                third.is<std::string>()
                    ? context.raw_get<sol::object>(third.as<std::string>())
                    : nilObject(lua);
            return runtimeResolverResult(lua, {value});
        }
        if (operation == "set") {
            if (!third.is<std::string>()) {
                throw std::invalid_argument(
                    "Node graph context key must be a string");
            }
            context.raw_set(third.as<std::string>(),
                            runtimeResolverArgument(lua, arguments, 4));
            return runtimeResolverResult(lua, {});
        }
        const sol::object rawGraph = context.raw_get<sol::object>("__graph__");
        const sol::object graph = rawGraph.is<sol::table>() ? rawGraph : first;
        if (operation == "getParent") {
            return runtimeResolverResult(
                lua,
                {protectedIndex(lua, graph,
                                sol::make_object(lua, std::string("parent")))});
        }
        if (operation == "setParent") {
            protectedAssign(lua, graph,
                            sol::make_object(lua, std::string("parent")),
                            third);
            return runtimeResolverResult(lua, {});
        }
    }
    sol::table context = lua.create_table();
    context[sol::metatable_key] = lua.create_table();
    sol::table graph = lua.create_table();
    graph.raw_set("parentClass", first);
    graph.raw_set("localGraph", context);
    sol::table parent = createWeakTable(lua, "v");
    parent.raw_set(1, second);
    sol::table graphMetatable = lua.create_table();
    graphMetatable.set_function(
        "__index",
        [parent](const sol::object&, const sol::object& key) -> sol::object {
            if (key.is<std::string>() && key.as<std::string>() == "parent") {
                return parent.raw_get<sol::object>(1);
            }
            return sol::make_object(parent.lua_state(), sol::lua_nil);
        });
    graphMetatable.set_function(
        "__newindex", [parent](sol::table target, const sol::object& key,
                               const sol::object& value) mutable {
            if (key.is<std::string>() && key.as<std::string>() == "parent") {
                parent.raw_set(1, value);
                return;
            }
            target.raw_set(key, value);
        });
    graph[sol::metatable_key] = graphMetatable;
    context.raw_set("__graph__", graph);
    registryTable(lua, NODEGRAPH_CONTEXTS_KEY, "k").raw_set(graph, context);
    return runtimeResolverResult(
        lua, {sol::make_object(lua, context), sol::make_object(lua, graph)});
}

sol::table invokeNodeGraphCallable(sol::state_view lua,
                                   const sol::table& arguments) {
    const sol::object callable = runtimeResolverArgument(lua, arguments, 1);
    const sol::object self = runtimeResolverArgument(lua, arguments, 2);
    const sol::object callArguments =
        runtimeResolverArgument(lua, arguments, 3);
    const sol::object rawContext = runtimeResolverArgument(lua, arguments, 4);
    sol::table refLocals = registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k");
    sol::object oldActive = nilObject(lua);
    sol::object oldRefLocal = refLocals.raw_get<sol::object>(callable);
    const bool hadRefLocal =
        oldRefLocal.valid() && oldRefLocal.get_type() != sol::type::lua_nil;
    bool hadActive = false;
    sol::table context = lua.create_table();
    if (rawContext.is<sol::table>()) {
        context = rawContext.as<sol::table>();
        oldActive = context.raw_get<sol::object>("__activeNodeFunction__");
        hadActive =
            oldActive.valid() && oldActive.get_type() != sol::type::lua_nil;
        context.raw_set("__activeNodeFunction__", callable);
        refLocals.raw_set(callable, context);
    }

    lua_State* state = lua.lua_state();
    const int base = lua_gettop(state);
    callable.push();
    int argumentCount = 0;
    if (self.valid() && self.get_type() != sol::type::lua_nil) {
        self.push();
        ++argumentCount;
    }
    if (callArguments.is<sol::table>()) {
        const sol::table values = callArguments.as<sol::table>();
        const sol::object rawCount = values.raw_get<sol::object>("n");
        const std::size_t count = rawCount.is<std::size_t>()
                                      ? rawCount.as<std::size_t>()
                                      : values.size();
        for (std::size_t index = 1; index <= count; ++index) {
            values.raw_get<sol::object>(index).push();
            ++argumentCount;
        }
    }
    const int status = isRuntimeCallable(lua, callable)
                           ? ludork::standard::protectedLuaCall(
                                 state, argumentCount, LUA_MULTRET)
                           : LUA_ERRRUN;
    std::string errorMessage;
    std::vector<sol::object> results;
    if (status == LUA_OK) {
        const int count = lua_gettop(state) - base;
        results.reserve(static_cast<std::size_t>(count));
        for (int index = 1; index <= count; ++index) {
            results.push_back(
                sol::stack::get<sol::object>(state, base + index));
        }
    } else if (lua_gettop(state) > base) {
        const char* rawError = lua_tostring(state, -1);
        errorMessage =
            rawError == nullptr ? "Node graph callable failed" : rawError;
    } else {
        errorMessage = "Node graph value is not callable";
    }
    lua_settop(state, base);

    if (rawContext.is<sol::table>()) {
        if (hadActive) {
            context.raw_set("__activeNodeFunction__", oldActive);
        } else {
            context.raw_set("__activeNodeFunction__", sol::lua_nil);
        }
        if (hadRefLocal) {
            refLocals.raw_set(callable, oldRefLocal);
        } else {
            refLocals.raw_set(callable, sol::lua_nil);
        }
    }
    if (status != LUA_OK) {
        throw std::runtime_error(errorMessage);
    }

    sol::table descriptor = lua.create_table();
    descriptor.raw_set("values", packedRuntimeObjects(lua, results));
    descriptor.raw_set("count", results.size());
    return runtimeResolverResult(lua, {sol::make_object(lua, descriptor)});
}

sol::table createNodeGraphNode(sol::state_view lua,
                               const sol::table& arguments) {
    const sol::object nodeModel = runtimeResolverArgument(lua, arguments, 1);
    if (nodeModel.get_type() == sol::type::lua_nil) {
        return runtimeResolverResult(lua, {});
    }
    const sol::object rawConstructor =
        ludork::standard::class_runtime::protectedGet(
            lua, nodeModel, sol::make_object(lua, "new"));
    if (!rawConstructor.is<sol::protected_function>()) {
        return runtimeResolverResult(lua, {});
    }
    sol::table constructorArguments = lua.create_table(5, 1);
    constructorArguments.raw_set("n", 5);
    for (std::size_t index = 1; index <= 5; ++index) {
        const sol::object value =
            runtimeResolverArgument(lua, arguments, index + 1);
        if (value.get_type() != sol::type::lua_nil) {
            constructorArguments.raw_set(index, value);
        }
    }
    const sol::table created = ludork::standard::class_runtime::invoke(
        lua, rawConstructor, constructorArguments);
    return runtimeResolverResult(lua,
                                 {runtimeResolverArgument(lua, created, 1)});
}

std::size_t nodeGraphPackedCount(const sol::table& values) {
    const sol::object rawCount = values.raw_get<sol::object>("n");
    return rawCount.is<std::size_t>() ? rawCount.as<std::size_t>()
                                      : values.size();
}

sol::table copyNodeGraphPackedValues(sol::state_view lua,
                                     const sol::object& rawValues,
                                     std::size_t count) {
    sol::table values = lua.create_table(static_cast<int>(count), 1);
    values.raw_set("n", count);
    if (rawValues.is<sol::table>()) {
        const sol::table source = rawValues.as<sol::table>();
        for (std::size_t index = 1; index <= count; ++index) {
            const sol::object value = source.raw_get<sol::object>(index);
            if (value.valid() && value.get_type() != sol::type::lua_nil) {
                values.raw_set(index, value);
            }
        }
    } else if (count > 0 && rawValues.valid() &&
               rawValues.get_type() != sol::type::lua_nil) {
        values.raw_set(1, rawValues);
    }
    return values;
}

bool isNodeGraphCacheKey(const sol::object& key) {
    if (key.is<std::string>()) {
        return true;
    }
    if (key.get_type() != sol::type::number) {
        return false;
    }
    lua_State* state = key.lua_state();
    key.push();
    const bool integer = lua_isinteger(state, -1) != 0;
    lua_pop(state, 1);
    return integer;
}

sol::table decodeNodeGraphCache(sol::state_view lua,
                                const sol::object& rawCache) {
    sol::table descriptors = lua.create_table();
    std::size_t descriptorIndex = 1;
    if (rawCache.is<sol::table>()) {
        for (const auto& entry : rawCache.as<sol::table>()) {
            if (!isNodeGraphCacheKey(entry.first)) {
                throw std::invalid_argument(
                    "Node graph cache keys must be strings or integers");
            }
            std::size_t count = 1;
            if (entry.second.is<sol::table>()) {
                count = nodeGraphPackedCount(entry.second.as<sol::table>());
            }
            sol::table descriptor = lua.create_table();
            descriptor.raw_set("key", entry.first);
            descriptor.raw_set(
                "values", copyNodeGraphPackedValues(lua, entry.second, count));
            descriptor.raw_set("count", count);
            descriptors.raw_set(descriptorIndex++, descriptor);
        }
    }
    descriptors.raw_set("n", descriptorIndex - 1);
    return runtimeResolverResult(lua, {sol::make_object(lua, descriptors)});
}

sol::table encodeNodeGraphCache(sol::state_view lua,
                                const sol::object& rawCache,
                                const sol::object& rawDescriptors) {
    if (!rawCache.is<sol::table>()) {
        return runtimeResolverResult(lua, {});
    }
    sol::table cache = rawCache.as<sol::table>();
    std::vector<sol::object> existingKeys;
    for (const auto& entry : cache) {
        existingKeys.push_back(entry.first);
    }
    for (const sol::object& key : existingKeys) {
        cache.raw_set(key, sol::lua_nil);
    }

    if (!rawDescriptors.is<sol::table>()) {
        return runtimeResolverResult(lua, {});
    }
    const sol::table descriptors = rawDescriptors.as<sol::table>();
    const std::size_t descriptorCount = nodeGraphPackedCount(descriptors);
    for (std::size_t index = 1; index <= descriptorCount; ++index) {
        const sol::object rawDescriptor =
            descriptors.raw_get<sol::object>(index);
        if (!rawDescriptor.is<sol::table>()) {
            throw std::invalid_argument(
                "Node graph cache descriptor must be a table");
        }
        const sol::table descriptor = rawDescriptor.as<sol::table>();
        const sol::object key = descriptor.raw_get<sol::object>("key");
        if (!isNodeGraphCacheKey(key)) {
            throw std::invalid_argument(
                "Node graph cache descriptor key must be a string or integer");
        }
        const sol::object rawValues = descriptor.raw_get<sol::object>("values");
        const sol::object rawCount = descriptor.raw_get<sol::object>("count");
        const std::size_t count =
            rawCount.is<std::size_t>() ? rawCount.as<std::size_t>()
            : rawValues.is<sol::table>()
                ? nodeGraphPackedCount(rawValues.as<sol::table>())
                : 0;
        cache.raw_set(key, copyNodeGraphPackedValues(lua, rawValues, count));
    }
    return runtimeResolverResult(lua, {});
}

sol::table bridgeNodeGraphCache(sol::state_view lua,
                                const sol::table& arguments) {
    const sol::object rawOperation = runtimeResolverArgument(lua, arguments, 1);
    if (!rawOperation.is<std::string>()) {
        throw std::invalid_argument(
            "Node graph cache operation must be a string");
    }
    const std::string operation = rawOperation.as<std::string>();
    if (operation == "decode") {
        return decodeNodeGraphCache(lua,
                                    runtimeResolverArgument(lua, arguments, 2));
    }
    if (operation == "encode") {
        return encodeNodeGraphCache(lua,
                                    runtimeResolverArgument(lua, arguments, 2),
                                    runtimeResolverArgument(lua, arguments, 3));
    }
    throw std::invalid_argument("Unknown node graph cache operation: " +
                                operation);
}

sol::table evaluateNodeGraphCondition(sol::state_view lua,
                                      const sol::table& arguments) {
    const sol::object condition = runtimeResolverArgument(lua, arguments, 1);
    const std::vector<sol::object> rawResults =
        callNodeGraphCallable(lua, condition, nilObject(lua), nilObject(lua));
    std::vector<sol::object> values;
    if (!rawResults.empty() &&
        rawResults.front().get_type() != sol::type::lua_nil) {
        const sol::object first = rawResults.front();
        if (first.is<sol::table>()) {
            const sol::table tableValue = first.as<sol::table>();
            const sol::object rawCount = tableValue.raw_get<sol::object>("n");
            if (rawCount.is<std::size_t>()) {
                const std::size_t count = rawCount.as<std::size_t>();
                values.reserve(count);
                for (std::size_t index = 1; index <= count; ++index) {
                    values.push_back(tableValue.raw_get<sol::object>(index));
                }
            } else {
                const std::size_t count = tableValue.size();
                if (count > 0) {
                    values.reserve(count);
                    for (std::size_t index = 1; index <= count; ++index) {
                        values.push_back(
                            tableValue.raw_get<sol::object>(index));
                    }
                } else {
                    for (const auto& entry : tableValue) {
                        values.push_back(entry.second);
                    }
                }
            }
        } else {
            values.push_back(first);
        }
    }
    bool finished = true;
    if (condition.is<sol::table>()) {
        const sol::object rawIsFinished =
            condition.as<sol::table>().get<sol::object>("isFinished");
        if (isRuntimeCallable(lua, rawIsFinished)) {
            const std::vector<sol::object> result = callNodeGraphCallable(
                lua, rawIsFinished, condition, nilObject(lua));
            finished = !result.empty() && luaBoolean(result.front());
        }
    }
    sol::table descriptor = lua.create_table();
    descriptor.raw_set("values", packedRuntimeObjects(lua, values));
    descriptor.raw_set("count", values.size());
    descriptor.raw_set("finished", finished);
    return runtimeResolverResult(lua, {sol::make_object(lua, descriptor)});
}

sol::table packArguments(sol::state_view lua,
                         const sol::variadic_args& arguments) {
    sol::table packed = lua.create_table(static_cast<int>(arguments.size()), 1);
    packed.raw_set("n", arguments.size());
    std::size_t index = 1;
    for (const sol::stack_proxy& argument : arguments) {
        packed.raw_set(index++, argument.get<sol::object>());
    }
    return packed;
}

sol::variadic_results unpackResults(sol::state_view lua,
                                    const sol::table& packed) {
    sol::variadic_results results;
    std::size_t count = packed.size();
    const sol::object rawCount = packed.raw_get<sol::object>("n");
    if (rawCount.is<std::size_t>()) {
        count = rawCount.as<std::size_t>();
    }
    results.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        results.push_back(runtimeResolverArgument(lua, packed, index));
    }
    return results;
}

void clearRuntimeServiceCaches(sol::state_view lua) {
    lua.registry().raw_set(BLUEPRINT_IMPLEMENTATION_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(CLASS_IDENTITY_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(CLASS_TYPE_METADATA_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(ATTR_METADATA_CACHE_KEY, sol::lua_nil);
    lua.registry().raw_set(COMPONENT_CACHES_KEY, sol::lua_nil);
    lua.registry().raw_set(NODEGRAPH_REF_LOCALS_KEY, sol::lua_nil);
    lua.registry().raw_set(NODEGRAPH_CONTEXTS_KEY, sol::lua_nil);
}

}  // namespace ludork::engine::runtime_detail
