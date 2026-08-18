#include <Runtime/EngineClassRuntime.hpp>

#include "EngineClassRuntimeInternal.hpp"
#include "RuntimeSubsystemServices.hpp"

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

using namespace ludork::engine::class_runtime_detail;

namespace {

constexpr std::array<const char*, 5> CLASS_RUNTIME_SERVICE_NAMES{
    "nodegraph.resolveClass",          "nodegraph.classData",
    "nodegraph.instantiateClassGraph", "nodegraph.classGraphHasExecutableEvent",
    "nodegraph.invalidateClass",
};

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
