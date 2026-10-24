#include <ClassServices.hpp>
#include <LuaError.hpp>
#include <LuaInvocation.hpp>

extern "C" {
#include <lua.h>
}

#include <stdexcept>
#include <string>

namespace ludork::standard::class_runtime {

int invoke(lua_State* state, const lua_glue::Object& callable,
           int argumentCount) {
    if (state == nullptr || argumentCount < 0 ||
        argumentCount > lua_gettop(state)) {
        throw std::invalid_argument("Invalid Lua invocation arguments");
    }
    if (!callable.is<lua_glue::Function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    const int stackBase = lua_gettop(state) - argumentCount;
    ludork::standard::ensureLuaStack(state, 1, "runtime callable");
    callable.push(state);
    lua_insert(state, stackBase + 1);
    const int status =
        ludork::standard::protectedLuaCall(state, argumentCount, LUA_MULTRET);
    try {
        ludork::standard::ensureLuaStack(state, LUA_MINSTACK,
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
