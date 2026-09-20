#include <LuaError.hpp>
#pragma once

#include <ClassRuntimeProtocol.hpp>

#include <LudorkRuntimeBinding/ValueCodec.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <stdexcept>
#include <string>

namespace ludork::runtime::binding {

namespace detail {

inline lua_glue::Object checkedResultValue(lua_glue::StateView lua,
                                           lua_glue::CallResult& result,
                                           int index) {
    if (!result.valid()) {
        const std::string error = result.error();
        throw std::runtime_error(error.c_str());
    }
    if (result.return_count() <= index) {
        return lua_glue::MakeObject(lua, lua_glue::nil);
    }
    return result.get<lua_glue::Object>(index);
}

}  // namespace detail

inline lua_glue::Table reverseLuaTable(lua_glue::StateView lua,
                                       const lua_glue::Table& source) {
    lua_glue::Table result = lua.create_table();
    const lua_glue::Object rawPairs =
        lua.globals().raw_get<lua_glue::Object>("pairs");
    if (!rawPairs.is<lua_glue::Function>()) {
        throw std::runtime_error("Lua pairs function is not defined");
    }
    lua_glue::Function pairs = rawPairs.as<lua_glue::Function>();
    lua_glue::CallResult initialized = pairs(source);
    const lua_glue::Object rawIterator =
        detail::checkedResultValue(lua, initialized, 0);
    lua_glue::Object iterationState =
        detail::checkedResultValue(lua, initialized, 1);
    lua_glue::Object control = detail::checkedResultValue(lua, initialized, 2);
    if (!rawIterator.is<lua_glue::Function>()) {
        throw std::runtime_error("Lua pairs iterator is not a function");
    }
    lua_glue::Function iterator = rawIterator.as<lua_glue::Function>();
    while (true) {
        lua_glue::CallResult next = iterator(iterationState, control);
        lua_glue::Object name = detail::checkedResultValue(lua, next, 0);
        if (isNil(name)) {
            break;
        }
        lua_glue::Object value = detail::checkedResultValue(lua, next, 1);
        if (!isNil(name) && !isNil(value)) {
            result.raw_set(value, name);
        }
        control = name;
    }
    return result;
}

namespace detail {

inline void pushArgumentOrNil(lua_State* state, int index) {
    if (index <= lua_gettop(state)) {
        lua_pushvalue(state, index);
    } else {
        lua_pushnil(state);
    }
}

inline bool pushRawTruthyField(lua_State* state, int tableIndex,
                               const char* name) {
    const int absoluteIndex = lua_absindex(state, tableIndex);
    lua_pushstring(state, name);
    lua_rawget(state, absoluteIndex);
    if (lua_toboolean(state, -1) != 0) {
        return true;
    }
    lua_pop(state, 1);
    return false;
}

inline void pushToString(lua_State* state, int index) {
    const int absoluteIndex = lua_absindex(state, index);
    lua_getglobal(state, "tostring");
    lua_pushvalue(state, absoluteIndex);
    if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
        throw std::runtime_error(ludork::standard::luaErrorMessage(state, -1));
    }
}

inline bool pushExpectedTypeName(lua_State* state, int expectedTypeIndex) {
    const int absoluteIndex = lua_absindex(state, expectedTypeIndex);
    constexpr const char* fields[] = {
        ludork::standard::class_runtime::protocol::CLASS_NAME_FIELD,
        "__blueprintClassPath"};
    for (const char* field : fields) {
        if (pushRawTruthyField(state, absoluteIndex, field)) {
            return true;
        }
    }
    lua_pushstring(
        state,
        ludork::standard::class_runtime::protocol::CLASS_METADATA_MODULE_FIELD);
    lua_rawget(state, absoluteIndex);
    if (lua_type(state, -1) != LUA_TNIL) {
        return true;
    }
    lua_pop(state, 1);

    lua_pushstring(
        state, ludork::standard::class_runtime::protocol::CLASS_MARKER_FIELD);
    lua_rawget(state, absoluteIndex);
    const bool isLuaClass = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (isLuaClass) {
        lua_pushliteral(state, "Lua class");
        return true;
    }
    return false;
}

inline int luaCastHelper(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        if (lua_type(state, 1) == LUA_TNONE || lua_isnil(state, 1)) {
            throw std::invalid_argument(
                "Error: targetType must be a type, but got nil");
        }
        pushArgumentOrNil(state, 2);
        return 1;
    });
}

inline int luaAssertTypeHelper(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        const int expectedType = lua_type(state, 2);
        if (expectedType == LUA_TSTRING) {
            int actualType = lua_type(state, 1);
            if (actualType == LUA_TNONE) {
                actualType = LUA_TNIL;
            }
            lua_pushstring(state, lua_typename(state, actualType));
            const bool matches =
                ludork::standard::compareLuaValues(state, 2, -1, LUA_OPEQ);
            lua_pop(state, 1);
            if (matches) {
                return 0;
            }
            lua_pushliteral(state, "Assert failed: expected ");
            lua_pushvalue(state, 2);
            lua_pushliteral(state, ", got ");
            lua_pushstring(state, lua_typename(state, actualType));
            lua_concat(state, 4);
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }

        if (expectedType == LUA_TTABLE) {
            lua_getglobal(state, "Class");
            lua_getfield(state, -1, "isInstance");
            lua_remove(state, -2);
            pushArgumentOrNil(state, 1);
            lua_pushvalue(state, 2);
            if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
                throw std::runtime_error(
                    ludork::standard::luaErrorMessage(state, -1));
            }
            const bool matches = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (matches) {
                return 0;
            }

            const bool hasExpectedName = pushExpectedTypeName(state, 2);
            if (!hasExpectedName || lua_toboolean(state, -1) == 0) {
                if (hasExpectedName) {
                    lua_pop(state, 1);
                }
                lua_pushvalue(state, 2);
            }
            pushToString(state, -1);
            lua_pushliteral(state, "Assert failed: value does not match ");
            lua_insert(state, -2);
            lua_concat(state, 2);
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }

        pushArgumentOrNil(state, 2);
        pushToString(state, -1);
        lua_pushliteral(state, "Assert failed: invalid type ");
        lua_insert(state, -2);
        lua_concat(state, 2);
        throw std::runtime_error(ludork::standard::luaErrorMessage(state, -1));
    });
}

inline int loadEvalChunk(lua_State* state, const char* expression,
                         std::size_t expressionLength) {
    std::string source("return ");
    source.append(expression, expressionLength);
    return luaL_loadbufferx(state, source.data(), source.size(), "=(Eval)",
                            "t");
}

inline int luaEvalHelper(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        const int argumentTop = lua_gettop(state);
        if (lua_type(state, 1) != LUA_TSTRING) {
            lua_pushnil(state);
            return 1;
        }

        std::size_t expressionLength = 0;
        const char* expression = lua_tolstring(state, 1, &expressionLength);
        if (expressionLength == 0) {
            lua_pushnil(state);
            return 1;
        }
        if (lua_isyieldable(state) != 0) {
            throw std::invalid_argument(
                "Eval cannot run from a yieldable coroutine");
        }

        lua_getglobal(state, "setmetatable");
        if (argumentTop >= 2 && lua_toboolean(state, 2) != 0) {
            lua_pushvalue(state, 2);
        } else {
            lua_createtable(state, 0, 0);
        }
        lua_createtable(state, 0, 1);
        lua_getglobal(state, "_G");
        lua_setfield(state, -2, "__index");
        if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        const int environmentIndex = lua_gettop(state);

        if (loadEvalChunk(state, expression, expressionLength) != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        const int functionIndex = lua_gettop(state);
        lua_pushvalue(state, environmentIndex);
        if (lua_setupvalue(state, functionIndex, 1) == nullptr) {
            lua_pop(state, 1);
            throw std::invalid_argument("Eval chunk has no environment");
        }

        lua_replace(state, 1);
        lua_settop(state, 1);
        if (lua_pcall(state, 0, LUA_MULTRET, 0) != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        return lua_gettop(state);
    });
}

inline lua_glue::Object luaCFunctionObject(lua_glue::StateView lua,
                                           lua_CFunction function) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, function);
    lua_glue::Object result = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return result;
}

}  // namespace detail

inline lua_glue::Object makeLuaCastHelper(lua_glue::StateView lua) {
    return detail::luaCFunctionObject(lua, detail::luaCastHelper);
}

inline lua_glue::Object makeLuaAssertTypeHelper(lua_glue::StateView lua) {
    return detail::luaCFunctionObject(lua, detail::luaAssertTypeHelper);
}

inline lua_glue::Object makeLuaEvalHelper(lua_glue::StateView lua) {
    return detail::luaCFunctionObject(lua, detail::luaEvalHelper);
}

}  // namespace ludork::runtime::binding
