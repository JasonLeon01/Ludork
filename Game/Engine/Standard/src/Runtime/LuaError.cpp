#include <LuaError.hpp>

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <stdexcept>
#include <cstdio>
#include <charconv>
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
    if (valueType == LUA_TSTRING) {
        std::size_t length = 0;
        const char* raw = lua_tolstring(state, absoluteIndex, &length);
        return std::string(raw, length);
    }
    if (valueType == LUA_TNUMBER) {
        char buffer[128]{};
        const auto converted =
            lua_isinteger(state, absoluteIndex)
                ? std::to_chars(buffer, buffer + sizeof(buffer),
                                lua_tointeger(state, absoluteIndex))
                : std::to_chars(buffer, buffer + sizeof(buffer),
                                lua_tonumber(state, absoluteIndex));
        return converted.ec == std::errc{} ? std::string(buffer, converted.ptr)
                                           : "<number>";
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
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        std::unordered_set<const void*> visited;
        const std::string message = describeLuaValue(state, 1, 0, visited);
        struct TracebackContext {
            const char* message;
        } context{message.c_str()};
        lua_glue::detail::ProtectedCallOperation(
            state,
            [](lua_State* target) {
                const auto* context = static_cast<const TracebackContext*>(
                    lua_touserdata(target, 1));
                luaL_traceback(target, target, context->message, 2);
                return 1;
            },
            &context, 1);
        return 1;
    });
}

}  // namespace

namespace ludork::standard {

bool compareLuaValues(lua_State* state, int leftIndex, int rightIndex,
                      int operation) {
    const int left = lua_absindex(state, leftIndex);
    const int right = lua_absindex(state, rightIndex);
    lua_glue::StackGuard stack(state);
    if (!lua_checkstack(state, 5)) {
        throw std::runtime_error("Lua stack cannot grow for comparison");
    }
    lua_pushcfunction(state, [](lua_State* target) {
        const int operation = static_cast<int>(lua_tointeger(target, 3));
        lua_pushboolean(target, lua_compare(target, 1, 2, operation));
        return 1;
    });
    lua_pushvalue(state, left);
    lua_pushvalue(state, right);
    lua_pushinteger(state, operation);
    if (protectedLuaCall(state, 3, 1) != LUA_OK) {
        throw std::runtime_error(luaErrorMessage(state, -1));
    }
    return lua_toboolean(state, -1) != 0;
}

int invokeLuaCallback(lua_State* state, const void* context,
                      int (*callback)(const void*)) {
    char message[4096]{};
    try {
        return callback(context);
    } catch (const std::exception& error) {
        std::snprintf(message, sizeof(message), "%s", error.what());
    } catch (...) {
        std::snprintf(message, sizeof(message), "%s",
                      "Native Lua callback failed");
    }
    lua_pushstring(state, message);
    return lua_error(state);
}

std::string luaErrorMessage(lua_State* state, int index) {
    std::unordered_set<const void*> visited;
    return describeLuaValue(state, index, 0, visited);
}

void installLuaErrorHandler(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    lua_glue::SetErrorHandler(state, luaTracebackHandler);
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
