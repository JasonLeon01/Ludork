#pragma once

#include <LudorkRuntimeBinding/ValueCodec.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <stdexcept>
#include <string>

namespace ludork::runtime::binding {

namespace detail {

inline sol::object checkedResultValue(sol::state_view lua,
                                      sol::protected_function_result& result,
                                      int index) {
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    if (result.return_count() <= index) {
        return sol::make_object(lua, sol::lua_nil);
    }
    return result.get<sol::object>(index);
}

}  // namespace detail

inline sol::table reverseLuaTable(sol::state_view lua,
                                  const sol::table& source) {
    sol::table result = lua.create_table();
    const sol::object rawPairs = lua.globals().raw_get<sol::object>("pairs");
    if (!rawPairs.is<sol::protected_function>()) {
        throw std::runtime_error("Lua pairs function is not defined");
    }
    sol::protected_function pairs = rawPairs.as<sol::protected_function>();
    sol::protected_function_result initialized = pairs(source);
    const sol::object rawIterator =
        detail::checkedResultValue(lua, initialized, 0);
    sol::object iterationState =
        detail::checkedResultValue(lua, initialized, 1);
    sol::object control = detail::checkedResultValue(lua, initialized, 2);
    if (!rawIterator.is<sol::protected_function>()) {
        throw std::runtime_error("Lua pairs iterator is not a function");
    }
    sol::protected_function iterator =
        rawIterator.as<sol::protected_function>();
    while (true) {
        sol::protected_function_result next = iterator(iterationState, control);
        sol::object name = detail::checkedResultValue(lua, next, 0);
        if (isNil(name)) {
            break;
        }
        sol::object value = detail::checkedResultValue(lua, next, 1);
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
    lua_call(state, 1, 1);
}

inline bool pushExpectedTypeName(lua_State* state, int expectedTypeIndex) {
    const int absoluteIndex = lua_absindex(state, expectedTypeIndex);
    constexpr const char* fields[] = {"__name", "__blueprintClassPath"};
    for (const char* field : fields) {
        if (pushRawTruthyField(state, absoluteIndex, field)) {
            return true;
        }
    }
    lua_pushliteral(state, "__metadataModule");
    lua_rawget(state, absoluteIndex);
    if (lua_type(state, -1) != LUA_TNIL) {
        return true;
    }
    lua_pop(state, 1);

    lua_pushliteral(state, "__ludorkClass");
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
    if (lua_type(state, 1) == LUA_TNONE || lua_isnil(state, 1)) {
        return luaL_error(state,
                          "Error: targetType must be a type, but got nil");
    }
    pushArgumentOrNil(state, 2);
    return 1;
}

inline int luaAssertTypeHelper(lua_State* state) {
    const int expectedType = lua_type(state, 2);
    if (expectedType == LUA_TSTRING) {
        int actualType = lua_type(state, 1);
        if (actualType == LUA_TNONE) {
            actualType = LUA_TNIL;
        }
        lua_pushstring(state, lua_typename(state, actualType));
        const bool matches = lua_compare(state, 2, -1, LUA_OPEQ) != 0;
        lua_pop(state, 1);
        if (matches) {
            return 0;
        }
        lua_pushliteral(state, "Assert failed: expected ");
        lua_pushvalue(state, 2);
        lua_pushliteral(state, ", got ");
        lua_pushstring(state, lua_typename(state, actualType));
        lua_concat(state, 4);
        return lua_error(state);
    }

    if (expectedType == LUA_TTABLE) {
        lua_getglobal(state, "Class");
        lua_getfield(state, -1, "isInstance");
        lua_remove(state, -2);
        pushArgumentOrNil(state, 1);
        lua_pushvalue(state, 2);
        lua_call(state, 2, 1);
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
        return lua_error(state);
    }

    pushArgumentOrNil(state, 2);
    pushToString(state, -1);
    lua_pushliteral(state, "Assert failed: invalid type ");
    lua_insert(state, -2);
    lua_concat(state, 2);
    return lua_error(state);
}

inline int loadEvalChunk(lua_State* state, const char* expression,
                         std::size_t expressionLength) {
    std::string source("return ");
    source.append(expression, expressionLength);
    return luaL_loadbufferx(state, source.data(), source.size(), "=(Eval)",
                            "t");
}

inline int luaEvalHelper(lua_State* state) {
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
        return luaL_error(state, "Eval cannot run from a yieldable coroutine");
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
    lua_call(state, 2, 1);
    const int environmentIndex = lua_gettop(state);

    if (loadEvalChunk(state, expression, expressionLength) != LUA_OK) {
        return lua_error(state);
    }
    const int functionIndex = lua_gettop(state);
    lua_pushvalue(state, environmentIndex);
    if (lua_setupvalue(state, functionIndex, 1) == nullptr) {
        lua_pop(state, 1);
        return luaL_error(state, "Eval chunk has no environment");
    }

    lua_replace(state, 1);
    lua_settop(state, 1);
    if (lua_pcall(state, 0, LUA_MULTRET, 0) != LUA_OK) {
        return lua_error(state);
    }
    return lua_gettop(state);
}

inline sol::object luaCFunctionObject(sol::state_view lua,
                                      lua_CFunction function) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, function);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return result;
}

}  // namespace detail

inline sol::object makeLuaCastHelper(sol::state_view lua) {
    return detail::luaCFunctionObject(lua, detail::luaCastHelper);
}

inline sol::object makeLuaAssertTypeHelper(sol::state_view lua) {
    return detail::luaCFunctionObject(lua, detail::luaAssertTypeHelper);
}

inline sol::object makeLuaEvalHelper(sol::state_view lua) {
    return detail::luaCFunctionObject(lua, detail::luaEvalHelper);
}

}  // namespace ludork::runtime::binding
