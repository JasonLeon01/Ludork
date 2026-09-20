#include "Class/ClassRuntimeInternals.hpp"
#include <JsonRuntimeProtocol.hpp>

#include "Composite/CompositeRuntime.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypedFields.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <ClassServices.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard::class_runtime::detail {

lua_glue::Table classFunction(lua_glue::ThisState state,
                              const lua_glue::Object& definition,
                              lua_glue::Arguments bases) {
    lua_glue::StateView lua(state);
    if (!definition.is<lua_glue::Table>()) {
        throw std::invalid_argument("Class definition must be a table");
    }
    lua_glue::Table baseList = lua.create_table();
    for (const lua_glue::StackValue& rawBase : bases) {
        const lua_glue::Object base = lua_glue::MakeObject(lua, rawBase);
        if (!base.is<lua_glue::Table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        baseList.add(base);
    }
    return finalizeClassImpl(definition.as<lua_glue::Table>(), baseList);
}

// ── Module-level functions (exposed on Class table)
// ───────────────────────────

namespace {

lua_glue::Table getParameterNames(const lua_glue::Object& callable) {
    lua_glue::StateView lua(callable.lua_state());
    if (callable.get_type() != lua_glue::Type::Function) {
        throw std::invalid_argument(
            "Class.getParameterNames requires a function");
    }
    const CallableInfo info = inspectCallable(callable);
    lua_glue::Table result = lua.create_table();
    for (const std::string& name : info.parameterNames) {
        result.add(name);
    }
    return result;
}

lua_glue::Object constructNamed(lua_glue::ThisState state,
                                const lua_glue::Object& rawType,
                                const lua_glue::Object& rawArguments) {
    lua_glue::StateView lua(state);
    if (!rawType.is<lua_glue::Table>()) {
        throw std::invalid_argument(
            "Class.constructNamed requires a class type");
    }
    lua_glue::Table arguments = lua.create_table();
    if (rawArguments.valid() &&
        rawArguments.get_type() != lua_glue::Type::Nil) {
        if (!rawArguments.is<lua_glue::Table>()) {
            throw std::invalid_argument(
                "Class.constructNamed arguments must be a table");
        }
        arguments = rawArguments.as<lua_glue::Table>();
    }
    const lua_glue::Table type = rawType.as<lua_glue::Table>();
    lua_glue::Object initializer = nilObject(lua);
    if (isClass(type)) {
        initializer =
            findScriptMember(lua, type, lua_glue::MakeObject(lua, "init"));
    } else {
        initializer = rawMember(lua, type, lua_glue::MakeObject(lua, "init"));
    }
    std::vector<lua_glue::Object> values;
    if (initializer.get_type() == lua_glue::Type::Function) {
        const CallableInfo info = inspectCallable(initializer);
        if (info.parameterNames.empty() && !tableIsEmpty(arguments)) {
            throw std::invalid_argument(
                "Class initializer exposes no named parameters");
        }
        values.reserve(info.parameterNames.size());
        for (const std::string& name : info.parameterNames) {
            const lua_glue::Object value =
                protectedIndex(lua, lua_glue::MakeObject(lua, arguments),
                               lua_glue::MakeObject(lua, name));
            values.push_back(value.valid() ? value : nilObject(lua));
        }
    } else if (!tableIsEmpty(arguments)) {
        throw std::invalid_argument(
            "Class without init does not accept named arguments");
    }
    const lua_glue::Object rawConstructor =
        protectedIndex(lua, rawType, lua_glue::MakeObject(lua, "new"));
    if (!rawConstructor.is<lua_glue::Function>()) {
        throw std::runtime_error("Class type has no new constructor");
    }
    lua_State* luaState = lua.lua_state();
    const int stackBase = lua_gettop(luaState);
    try {
        const int resultCount = invokeRuntimeFunction(
            lua, rawConstructor, values, "named constructor arguments");
        lua_glue::Object result =
            resultCount == 0
                ? nilObject(lua)
                : lua_glue::Read<lua_glue::Object>(luaState, stackBase + 1);
        lua_settop(luaState, stackBase);
        return result;
    } catch (...) {
        lua_settop(luaState, stackBase);
        throw;
    }
}

bool isSubclass(lua_glue::ThisState state, const lua_glue::Table& value,
                const lua_glue::Table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(
        lua_glue::StateView(state), value, targetClass);
}

bool isInstance(lua_glue::ThisState state, const lua_glue::Object& value,
                const lua_glue::Object& target) {
    lua_glue::StateView lua(state);
    if (target.is<std::string>()) {
        return target.as<std::string>() ==
               lua_glue::TypeName(lua.lua_state(), value.get_type());
    }
    if (!target.is<lua_glue::Table>()) {
        throw std::invalid_argument(
            "Class.isInstance target must be a class or Lua type name");
    }
    return ludork::standard::class_runtime::isInstanceOf(
        lua, value, target.as<lua_glue::Table>());
}

lua_glue::Object classType(lua_glue::ThisState state,
                           const lua_glue::Object& value) {
    return ludork::standard::class_runtime::typeOf(lua_glue::StateView(state),
                                                   value);
}

bool hasOwnFieldFunction(lua_glue::ThisState state,
                         const lua_glue::Object& target,
                         const lua_glue::Object& key) {
    return hasRawOwnField(lua_glue::StateView(state), target, key);
}

lua_glue::Table getMroFunction(lua_glue::ThisState state,
                               const lua_glue::Object& value) {
    return mroCopy(lua_glue::StateView(state), value);
}

lua_glue::Object copyFunction(lua_glue::ThisState state,
                              const lua_glue::Object& value) {
    return class_runtime::shallowCopy(lua_glue::StateView(state), value);
}

lua_glue::Object deepCopyFunction(lua_glue::ThisState state,
                                  const lua_glue::Object& value) {
    return class_runtime::deepCopy(lua_glue::StateView(state), value);
}

}  // namespace

// ── Module entry point
// ────────────────────────────────────────────────────────

lua_glue::Table createModule(lua_glue::StateView lua) {
    registerNativeInterop(lua.lua_state());
    lua.registry().raw_set(SHUTTING_DOWN_KEY, lua_glue::nil);
    lua_glue::Table root = lua.create_table();
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

lua_glue::Table createModule(lua_glue::StateView lua) {
    return detail::createModule(lua);
}

void shutdown(lua_State* state) noexcept {
    using namespace detail;
    if (state == nullptr) {
        return;
    }
    const int stackTop = lua_gettop(state);
    clearExplicitNilFields(state);
    lua_pushboolean(state, 1);
    lua_setfield(state, LUA_REGISTRYINDEX, SHUTTING_DOWN_KEY);
    constexpr const char* registryKeys[] = {
        METHOD_OWNERS_KEY,
        NATIVE_PROPERTY_CACHE_KEY,
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
        ludork::standard::json_runtime::protocol::JSON_NULL_KEY,
        ludork::standard::json_runtime::protocol::JSON_ARRAY_METATABLE_KEY,
        ludork::standard::json_runtime::protocol::
            JSON_EMPTY_ARRAY_METATABLE_KEY,
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
