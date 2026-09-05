#include <LuaError.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {

std::string describeLuaValue(lua_State* state, int index, int depth,
                             std::unordered_set<const void*>& visited) {
    const int absoluteIndex = lua_absindex(state, index);
    const int valueType = lua_type(state, absoluteIndex);
    if (valueType == LUA_TNIL) {
        return "nil";
    }
    if (valueType == LUA_TBOOLEAN) {
        return lua_toboolean(state, absoluteIndex) != 0 ? "true" : "false";
    }
    if (valueType == LUA_TSTRING || valueType == LUA_TNUMBER) {
        lua_pushvalue(state, absoluteIndex);
        std::size_t length = 0;
        const char* raw = lua_tolstring(state, -1, &length);
        const std::string result = raw == nullptr
                                       ? lua_typename(state, valueType)
                                       : std::string(raw, length);
        lua_pop(state, 1);
        return result;
    }
    if (valueType != LUA_TTABLE) {
        return lua_typename(state, valueType);
    }
    const void* identity = lua_topointer(state, absoluteIndex);
    if (visited.contains(identity)) {
        return "<cycle>";
    }
    if (depth >= 4) {
        return "<max-depth>";
    }
    visited.insert(identity);
    std::string result = "{";
    bool first = true;
    int count = 0;
    lua_pushnil(state);
    while (lua_next(state, absoluteIndex) != 0) {
        if (!first) {
            result += ", ";
        }
        result += describeLuaValue(state, -2, depth + 1, visited);
        result += "=";
        result += describeLuaValue(state, -1, depth + 1, visited);
        first = false;
        ++count;
        lua_pop(state, 1);
        if (count >= 32) {
            result += ", ...";
            lua_pop(state, 1);
            break;
        }
    }
    visited.erase(identity);
    result += "}";
    return result;
}

int luaTracebackHandler(lua_State* state) {
    std::unordered_set<const void*> visited;
    const std::string message = describeLuaValue(state, 1, 0, visited);
    luaL_traceback(state, state, message.c_str(), 1);
    return 1;
}

}  // namespace

namespace ludork::standard {

std::string luaErrorMessage(lua_State* state, int index) {
    std::unordered_set<const void*> visited;
    return describeLuaValue(state, index, 0, visited);
}

void installLuaErrorHandler(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    lua_pushcfunction(state, luaTracebackHandler);
    sol::protected_function::set_default_handler(sol::stack_object(state, -1));
    lua_pop(state, 1);
}

int protectedLuaCall(lua_State* state, int argumentCount, int resultCount) {
    const int functionIndex = lua_gettop(state) - argumentCount;
    if (lua_checkstack(state, 1) == 0) {
        throw std::runtime_error("Lua stack cannot grow for protected call");
    }
    lua_pushcfunction(state, luaTracebackHandler);
    lua_insert(state, functionIndex);
    const int status =
        lua_pcall(state, argumentCount, resultCount, functionIndex);
    lua_remove(state, functionIndex);
    return status;
}

}  // namespace ludork::standard
