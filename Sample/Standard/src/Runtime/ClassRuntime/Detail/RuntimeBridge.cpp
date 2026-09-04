#include "Detail/RuntimeBridge.hpp"

#include <ClassServices.hpp>
#include <LuaError.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace ludork::standard::class_runtime::detail {

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context) {
    if (count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error(std::string(context) + " count overflow");
    }
    if (count != 0 && lua_checkstack(state, static_cast<int>(count)) == 0) {
        throw std::runtime_error(std::string("Lua stack cannot grow for ") +
                                 context);
    }
}

int invokeRuntimeFunction(sol::state_view lua, const sol::object& rawCallable,
                          const std::vector<sol::object>& arguments,
                          const char* context) {
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        if (arguments.size() > static_cast<std::size_t>(INT_MAX - 1)) {
            throw std::length_error(std::string(context) + " count overflow");
        }
        ensureRuntimeLuaStack(state, arguments.size() + 1, context);
        rawCallable.push();
        for (const sol::object& argument : arguments) {
            argument.push();
        }
        const int status = ludork::standard::protectedLuaCall(
            state, static_cast<int>(arguments.size()), LUA_MULTRET);
        ensureRuntimeLuaStack(state, LUA_MINSTACK, "runtime function results");
        if (status != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        return lua_gettop(state) - stackBase;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

int invoke(lua_State* state, const sol::object& callable, int argumentCount) {
    if (state == nullptr || argumentCount < 0 ||
        argumentCount > lua_gettop(state)) {
        throw std::invalid_argument("Invalid Lua invocation arguments");
    }
    if (!callable.is<sol::protected_function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    const int stackBase = lua_gettop(state) - argumentCount;
    detail::ensureRuntimeLuaStack(state, 1, "runtime callable");
    callable.push();
    lua_insert(state, stackBase + 1);
    const int status =
        ludork::standard::protectedLuaCall(state, argumentCount, LUA_MULTRET);
    try {
        detail::ensureRuntimeLuaStack(state, LUA_MINSTACK,
                                      "runtime callable results");
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
    if (status != LUA_OK) {
        const std::string message =
            ludork::standard::luaErrorMessage(state, -1);
        lua_settop(state, stackBase);
        throw std::runtime_error(message);
    }
    return lua_gettop(state) - stackBase;
}

}  // namespace ludork::standard::class_runtime
