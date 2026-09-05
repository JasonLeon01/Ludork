#include "Class/ClassRuntimeInternals.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Instance/InstanceRuntime.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaError.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Callable introspection
// ────────────────────────────────────────────────────

CallableInfo inspectCallable(const sol::object& callable) {
    CallableInfo result;
    if (callable.get_type() != sol::type::function) {
        return result;
    }
    lua_State* state = callable.lua_state();
    callable.push();
    lua_Debug info{};
    if (lua_getinfo(state, ">u", &info) == 0) {
        return result;
    }
    result.parameterCount = static_cast<int>(info.nparams);
    result.vararg = info.isvararg != 0;
    callable.push();
    result.parameterNames.reserve(info.nparams);
    for (int index = 1; index <= static_cast<int>(info.nparams); ++index) {
        const char* name = lua_getlocal(state, nullptr, index);
        if (name != nullptr && std::string(name) != "self") {
            result.parameterNames.emplace_back(name);
        }
    }
    lua_pop(state, 1);
    return result;
}

sol::table constructorClass(lua_State* state) {
    return sol::stack::get<sol::table>(state, lua_upvalueindex(1));
}

// ── Class construction entry points ──────────────────────────────────────────

namespace {

void callConstructorFunction(lua_State* state, const sol::object& function,
                             const sol::object& instance, int firstArgument,
                             int originalTop) {
    const CallableInfo info = inspectCallable(function);
    const int argumentCount =
        firstArgument <= originalTop ? originalTop - firstArgument + 1 : 0;
    const int maximumArgumentCount = std::max(0, info.parameterCount - 1);
    if (!info.vararg && argumentCount > maximumArgumentCount) {
        throw std::invalid_argument(
            "Class initializer received too many arguments");
    }
    ensureRuntimeLuaStack(state, static_cast<std::size_t>(argumentCount) + 2,
                          "class initializer arguments");
    function.push();
    instance.push();
    for (int index = firstArgument; index <= originalTop; ++index) {
        lua_pushvalue(state, index);
    }
    if (ludork::standard::protectedLuaCall(
            state, originalTop - firstArgument + 2, 0) != LUA_OK) {
        size_t length = 0;
        const char* message = luaL_tolstring(state, -1, &length);
        const std::string error = message == nullptr
                                      ? "Class initializer failed"
                                      : std::string(message, length);
        lua_pop(state, 2);
        throw std::runtime_error(error);
    }
}

int constructClassInstance(lua_State* state, int firstArgument) {
    sol::state_view lua(state);
    const int originalTop = lua_gettop(state);
    const sol::table classTable = constructorClass(state);
    const sol::object initializer =
        findScriptMember(lua, classTable, sol::make_object(lua, "init"));
    const bool hasInitializer = initializer.is<sol::function>();
    sol::object instance;
    if (!hasInitializer && firstArgument <= originalTop) {
        const std::vector<sol::table> roots = nativeRoots(lua, classTable);
        if (roots.size() > 1) {
            throw std::invalid_argument(
                "Class with multiple native roots and constructor arguments "
                "must define init");
        }
        if (roots.size() == 1) {
            sol::table arguments = lua.create_table();
            const int argumentCount = originalTop - firstArgument + 1;
            arguments.raw_set("n", argumentCount);
            for (int index = firstArgument; index <= originalTop; ++index) {
                arguments.raw_set(index - firstArgument + 1,
                                  sol::stack::get<sol::object>(state, index));
            }
            sol::table constructorArguments = lua.create_table();
            constructorArguments.raw_set(roots.front(), arguments);
            instance = allocateInstance(lua, classTable, constructorArguments);
        } else {
            throw std::invalid_argument(
                "Class without init does not accept constructor arguments");
        }
    } else {
        instance =
            allocateInstance(lua, classTable, sol::object(), hasInitializer);
    }
    try {
        validateNativeInstanceShape(lua, classTable, instance);
        syncNativeClassDefaults(lua, classTable, instance);
        if (hasInitializer) {
            callConstructorFunction(state, initializer, instance, firstArgument,
                                    originalTop);
        }
        if (hasInitializer) {
            completeDefaultNativeRoots(lua, classTable, instance);
        }
        validateNativeRoots(lua, classTable, instance);
        finishNativeConstruction(lua, classTable, instance);
    } catch (...) {
        failNativeConstruction(lua, classTable, instance);
        throw;
    }
    instance.push();
    return 1;
}

}  // namespace

int classNew(lua_State* state) {
    try {
        return constructClassInstance(state, 1);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classCall(lua_State* state) {
    try {
        return constructClassInstance(state, 2);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

}  // namespace ludork::standard::class_runtime::detail
