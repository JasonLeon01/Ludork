#include <LuaInvocation.hpp>

#include <LuaError.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <stdexcept>
#include <string>

namespace ludork::standard {

void ensureLuaStack(lua_State* state, std::size_t count, const char* context) {
    if (count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error(std::string(context) + " count overflow");
    }
    if (count != 0 && lua_checkstack(state, static_cast<int>(count)) == 0) {
        throw std::runtime_error(std::string("Lua stack cannot grow for ") +
                                 context);
    }
}

int invokeLuaFunction(lua_State* state, const lua_glue::Object& callable,
                      const std::vector<lua_glue::Object>& arguments,
                      const char* context) {
    const int stackBase = lua_gettop(state);
    try {
        if (arguments.size() > static_cast<std::size_t>(INT_MAX - 1)) {
            throw std::length_error(std::string(context) + " count overflow");
        }
        ensureLuaStack(state, arguments.size() + 1, context);
        callable.push(state);
        for (const lua_glue::Object& argument : arguments) {
            argument.push(state);
        }
        const int status = protectedLuaCall(
            state, static_cast<int>(arguments.size()), LUA_MULTRET);
        ensureLuaStack(state, LUA_MINSTACK, "runtime function results");
        if (status != LUA_OK) {
            throw std::runtime_error(luaErrorMessage(state, -1));
        }
        return lua_gettop(state) - stackBase;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

}  // namespace ludork::standard
