#include "Class/ClassRuntimeInternals.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <ClassServices.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard::class_runtime::detail {

sol::table classFunction(sol::this_state state, const sol::object& definition,
                         sol::variadic_args bases) {
    sol::state_view lua(state);
    if (!definition.is<sol::table>()) {
        throw std::invalid_argument("Class definition must be a table");
    }
    sol::table baseList = lua.create_table();
    for (const sol::stack_proxy& rawBase : bases) {
        const sol::object base = sol::make_object(lua, rawBase);
        if (!base.is<sol::table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        baseList.add(base);
    }
    return finalizeClassImpl(definition.as<sol::table>(), baseList);
}

// ── Module-level functions (exposed on Class table)
// ───────────────────────────

namespace {

sol::table getParameterNames(const sol::object& callable) {
    sol::state_view lua(callable.lua_state());
    if (callable.get_type() != sol::type::function) {
        throw std::invalid_argument(
            "Class.getParameterNames requires a function");
    }
    const CallableInfo info = inspectCallable(callable);
    sol::table result = lua.create_table();
    for (const std::string& name : info.parameterNames) {
        result.add(name);
    }
    return result;
}

sol::object constructNamed(sol::this_state state, const sol::object& rawType,
                           const sol::object& rawArguments) {
    sol::state_view lua(state);
    if (!rawType.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.constructNamed requires a class type");
    }
    sol::table arguments = lua.create_table();
    if (rawArguments.valid() && rawArguments.get_type() != sol::type::lua_nil) {
        if (!rawArguments.is<sol::table>()) {
            throw std::invalid_argument(
                "Class.constructNamed arguments must be a table");
        }
        arguments = rawArguments.as<sol::table>();
    }
    const sol::table type = rawType.as<sol::table>();
    sol::object initializer = nilObject(lua);
    if (isClass(type)) {
        initializer =
            findScriptMember(lua, type, sol::make_object(lua, "init"));
    } else {
        initializer = rawMember(lua, type, sol::make_object(lua, "init"));
    }
    std::vector<sol::object> values;
    if (initializer.get_type() == sol::type::function) {
        const CallableInfo info = inspectCallable(initializer);
        if (info.parameterNames.empty() && !tableIsEmpty(arguments)) {
            throw std::invalid_argument(
                "Class initializer exposes no named parameters");
        }
        values.reserve(info.parameterNames.size());
        for (const std::string& name : info.parameterNames) {
            const sol::object value =
                protectedIndex(lua, sol::make_object(lua, arguments),
                               sol::make_object(lua, name));
            values.push_back(value.valid() ? value : nilObject(lua));
        }
    } else if (!tableIsEmpty(arguments)) {
        throw std::invalid_argument(
            "Class without init does not accept named arguments");
    }
    const sol::object rawConstructor =
        protectedIndex(lua, rawType, sol::make_object(lua, "new"));
    if (!rawConstructor.is<sol::protected_function>()) {
        throw std::runtime_error("Class type has no new constructor");
    }
    lua_State* luaState = lua.lua_state();
    const int stackBase = lua_gettop(luaState);
    try {
        const int resultCount = invokeRuntimeFunction(
            lua, rawConstructor, values, "named constructor arguments");
        sol::object result = resultCount == 0 ? nilObject(lua)
                                              : sol::stack::get<sol::object>(
                                                    luaState, stackBase + 1);
        lua_settop(luaState, stackBase);
        return result;
    } catch (...) {
        lua_settop(luaState, stackBase);
        throw;
    }
}

bool isSubclass(sol::this_state state, const sol::table& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(sol::state_view(state),
                                                         value, targetClass);
}

bool isInstance(sol::this_state state, const sol::object& value,
                const sol::object& target) {
    sol::state_view lua(state);
    if (target.is<std::string>()) {
        return target.as<std::string>() ==
               sol::type_name(lua.lua_state(), value.get_type());
    }
    if (!target.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.isInstance target must be a class or Lua type name");
    }
    return ludork::standard::class_runtime::isInstanceOf(
        lua, value, target.as<sol::table>());
}

sol::object classType(sol::this_state state, const sol::object& value) {
    return ludork::standard::class_runtime::typeOf(sol::state_view(state),
                                                   value);
}

bool hasOwnFieldFunction(sol::this_state state, const sol::object& target,
                         const sol::object& key) {
    return hasRawOwnField(sol::state_view(state), target, key);
}

sol::table getMroFunction(sol::this_state state, const sol::object& value) {
    return mroCopy(sol::state_view(state), value);
}

sol::object copyFunction(sol::this_state state, const sol::object& value) {
    return class_runtime::shallowCopy(sol::state_view(state), value);
}

sol::object deepCopyFunction(sol::this_state state, const sol::object& value) {
    return class_runtime::deepCopy(sol::state_view(state), value);
}

}  // namespace

// ── Module entry point
// ────────────────────────────────────────────────────────

sol::table createModule(sol::state_view lua) {
    lua.registry().raw_set(SHUTTING_DOWN_KEY, sol::lua_nil);
    sol::table root = lua.create_table();
    root.set_function("isInstance", &isInstance);
    root.set_function("isSubclass", &isSubclass);
    root.set_function("type", &classType);
    root.set_function("hasOwnField", &hasOwnFieldFunction);
    root.set_function("getMro", &getMroFunction);
    root.set_function("getParameterNames", &getParameterNames);
    root.set_function("constructNamed", &constructNamed);
    root.set_function("super", superFunction);
    root.set_function("monitor", &registerMonitor);
    root.set_function("unmonitor", &unregisterMonitor);
    root.raw_set("MISSING", lua.create_table());
    lua.globals().set_function("class", &classFunction);
    lua.globals().set_function("copy", &copyFunction);
    lua.globals().set_function("deepcopy", &deepCopyFunction);
    lua["super"] = root["super"];
    return root;
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

sol::table createModule(sol::state_view lua) {
    return detail::createModule(lua);
}

void shutdown(lua_State* state) noexcept {
    using namespace detail;
    if (state == nullptr) {
        return;
    }
    const int stackTop = lua_gettop(state);
    lua_pushboolean(state, 1);
    lua_setfield(state, LUA_REGISTRYINDEX, SHUTTING_DOWN_KEY);
    constexpr const char* registryKeys[] = {
        METHOD_OWNERS_KEY,
        NATIVE_TYPE_CACHE_KEY,
        INSTANCES_KEY,
        COMPOSITE_METATABLE_KEY,
        CONSTRUCTING_COMPOSITE_METATABLE_KEY,
        NATIVE_OWNERS_KEY,
        protocol::NATIVE_POINTER_OWNERS_REGISTRY_KEY,
        protocol::DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY,
        SUPER_PROXY_CACHE_KEY,
        SUPER_PROXY_METATABLE_KEY,
        MONITOR_STATES_KEY,
        LIFECYCLE_STATES_KEY,
        DISPOSED_METATABLE_KEY,
        "LuaSF.JsonNullSentinel",
        "LuaSF.JsonArrayMetatable",
        "LuaSF.JsonEmptyArrayMetatable",
    };
    for (const char* key : registryKeys) {
        lua_pushnil(state);
        lua_setfield(state, LUA_REGISTRYINDEX, key);
    }
    lua_pushlightuserdata(
        state, static_cast<void*>(&nativeDeepCopyProtocolsKeyStorage));
    lua_pushnil(state);
    lua_rawset(state, LUA_REGISTRYINDEX);
    constexpr const char* globalKeys[] = {
        "Class",    "class", "copy",
        "deepcopy", "super", "_LUDORK_STANDARD_UPDATE",
    };
    for (const char* key : globalKeys) {
        lua_pushnil(state);
        lua_setglobal(state, key);
    }
    lua_settop(state, stackTop);
}

}  // namespace ludork::standard::class_runtime
