#include "ContainerRuntimeInternal.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

lua_Integer checkedIndex(const sol::object& value, const char* name) {
    if (value.get_type() != sol::type::number) {
        throw std::invalid_argument(std::string(name) + " must be an integer");
    }
    lua_State* state = value.lua_state();
    value.push();
    int valid = 0;
    const lua_Integer result = lua_tointegerx(state, -1, &valid);
    lua_pop(state, 1);
    if (valid == 0) {
        throw std::invalid_argument(std::string(name) + " must be an integer");
    }
    return result;
}

sol::object typeMember(sol::state_view lua, const char* typeName,
                       const sol::object& key) {
    const sol::object rawType = lua.globals().raw_get<sol::object>(typeName);
    if (!rawType.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::object result = rawType.as<sol::table>().get<sol::object>(key);
    return result.valid() ? result : nilObject(lua);
}

sol::object sequenceIndex(sol::this_state state, const sol::object& self,
                          const sol::object& key, const char* typeName,
                          std::size_t length) {
    sol::state_view lua(state);
    if (key.get_type() == sol::type::number) {
        const lua_Integer index = checkedIndex(key, "sequence index");
        if (index < 1 || static_cast<std::size_t>(index) > length) {
            return nilObject(lua);
        }
        return exposedValue(lua,
                            sequenceValues(self).raw_get<sol::object>(index));
    }
    const sol::object member = typeMember(lua, typeName, key);
    if (member.valid() && member.get_type() != sol::type::lua_nil) {
        return member;
    }
    return nilObject(lua);
}

int lessThan(lua_State* state) {
    lua_pushboolean(state, lua_compare(state, 1, 2, LUA_OPLT));
    return 1;
}

bool callComparator(const sol::protected_function& comparator,
                    const sol::object& left, const sol::object& right) {
    sol::protected_function_result result = comparator(left, right);
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    if (result.get_type() != sol::type::boolean) {
        throw std::invalid_argument("list sort comparator must return boolean");
    }
    return result.get<bool>();
}

int sequenceIteratorBody(lua_State* state) {
    sol::state_view lua(state);
    const sol::object self =
        sol::stack::get<sol::object>(state, lua_upvalueindex(1));
    const lua_Integer expectedVersion =
        lua_tointeger(state, lua_upvalueindex(2));
    lua_Integer cursor = lua_tointeger(state, lua_upvalueindex(3));
    std::size_t length = 0;
    if (self.is<NativeList>()) {
        const NativeList& list = self.as<NativeList&>();
        if (list.version != static_cast<std::uint64_t>(expectedVersion)) {
            return luaL_error(state, "list changed structure during iteration");
        }
        length = list.length;
    } else if (self.is<NativeTuple>()) {
        length = self.as<NativeTuple&>().length;
    } else {
        return luaL_error(state, "sequence iterator target is invalid");
    }
    ++cursor;
    if (cursor < 1 || static_cast<std::size_t>(cursor) > length) {
        return 0;
    }
    lua_pushinteger(state, cursor);
    lua_replace(state, lua_upvalueindex(3));
    lua_pushinteger(state, cursor);
    exposedValue(lua, sequenceValues(self).raw_get<sol::object>(cursor)).push();
    return 2;
}

int sequenceIterator(lua_State* state) {
    try {
        return sequenceIteratorBody(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int dictIteratorBody(lua_State* state) {
    sol::state_view lua(state);
    const sol::object self =
        sol::stack::get<sol::object>(state, lua_upvalueindex(1));
    NativeDict& dict = self.as<NativeDict&>();
    const lua_Integer expectedVersion =
        lua_tointeger(state, lua_upvalueindex(2));
    if (dict.version != static_cast<std::uint64_t>(expectedVersion)) {
        return luaL_error(state, "dict changed structure during iteration");
    }
    lua_Integer cursor = lua_tointeger(state, lua_upvalueindex(3));
    while (++cursor <= static_cast<lua_Integer>(dict.entries.size())) {
        if (!dict.entries[static_cast<std::size_t>(cursor - 1)].alive) {
            continue;
        }
        lua_pushinteger(state, cursor);
        lua_replace(state, lua_upvalueindex(3));
        dictKeys(self).raw_get<sol::object>(cursor).push();
        dictEntryValue(lua, self, static_cast<std::size_t>(cursor - 1)).push();
        return 2;
    }
    return 0;
}

int dictIterator(lua_State* state) {
    try {
        return dictIteratorBody(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

void pushSequenceIterator(lua_State* state, int selfIndex) {
    const int absoluteIndex = lua_absindex(state, selfIndex);
    const sol::object self = sol::stack::get<sol::object>(state, absoluteIndex);
    const std::uint64_t version =
        self.is<NativeList>() ? self.as<NativeList&>().version : 0;
    lua_pushvalue(state, absoluteIndex);
    lua_pushinteger(state, static_cast<lua_Integer>(version));
    lua_pushinteger(state, 0);
    lua_pushcclosure(state, sequenceIterator, 3);
}

void pushDictIterator(lua_State* state, int selfIndex) {
    const int absoluteIndex = lua_absindex(state, selfIndex);
    const sol::object self = sol::stack::get<sol::object>(state, absoluteIndex);
    lua_pushvalue(state, absoluteIndex);
    lua_pushinteger(state,
                    static_cast<lua_Integer>(self.as<NativeDict&>().version));
    lua_pushinteger(state, 0);
    lua_pushcclosure(state, dictIterator, 3);
}

int nativeIpairsBody(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    const sol::object target = sol::stack::get<sol::object>(state, 1);
    if (target.is<NativeList>() || target.is<NativeTuple>()) {
        pushSequenceIterator(state, 1);
        lua_pushnil(state);
        lua_pushnil(state);
        return 3;
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushvalue(state, 1);
    lua_call(state, 1, LUA_MULTRET);
    return lua_gettop(state) - argumentCount;
}

int nativeIpairs(lua_State* state) {
    try {
        return nativeIpairsBody(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int typeTableIndex(lua_State* state) {
    if (lua_type(state, 2) == LUA_TSTRING) {
        std::size_t length = 0;
        const char* key = lua_tolstring(state, 2, &length);
        if (length == 3 && std::memcmp(key, "new", 3) == 0) {
            lua_pushnil(state);
            return 1;
        }
    }
    const int originalType = lua_type(state, lua_upvalueindex(1));
    if (originalType == LUA_TFUNCTION) {
        lua_pushvalue(state, lua_upvalueindex(1));
        lua_pushvalue(state, 1);
        lua_pushvalue(state, 2);
        lua_call(state, 2, 1);
        return 1;
    }
    if (originalType == LUA_TTABLE) {
        lua_pushvalue(state, lua_upvalueindex(1));
        lua_pushvalue(state, 2);
        lua_rawget(state, -2);
        lua_remove(state, -2);
        return 1;
    }
    lua_pushnil(state);
    return 1;
}

void maskNewConstructor(sol::table typeTable) {
    lua_State* state = typeTable.lua_state();
    typeTable.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        throw std::runtime_error("Native container type metatable is missing");
    }
    lua_getfield(state, -1, "__index");
    lua_pushcclosure(state, typeTableIndex, 1);
    lua_setfield(state, -2, "__index");
    lua_pop(state, 2);
}

std::tuple<sol::function, sol::object, sol::object> sequencePairs(
    const sol::object& self, sol::this_state state) {
    lua_State* luaState = state;
    self.push();
    pushSequenceIterator(luaState, -1);
    sol::function iterator = sol::stack::get<sol::function>(luaState, -1);
    lua_pop(luaState, 2);
    sol::state_view lua(luaState);
    return std::make_tuple(iterator, nilObject(lua), nilObject(lua));
}

std::tuple<sol::function, sol::object, sol::object> nativeDictPairs(
    const sol::object& self, sol::this_state state) {
    lua_State* luaState = state;
    self.push();
    pushDictIterator(luaState, -1);
    sol::function iterator = sol::stack::get<sol::function>(luaState, -1);
    lua_pop(luaState, 2);
    sol::state_view lua(luaState);
    return std::make_tuple(iterator, nilObject(lua), nilObject(lua));
}

void overrideNewIndex(const sol::object& sample, lua_CFunction newIndex) {
    lua_State* state = sample.lua_state();
    sample.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        throw std::runtime_error(
            "Native container instance metatable is missing");
    }
    lua_pushcfunction(state, newIndex);
    lua_setfield(state, -2, "__newindex");
    lua_pop(state, 2);
}

void registerIpairs(sol::state_view lua) {
    sol::table registry = lua.registry();
    sol::object original = registry.raw_get<sol::object>(ORIGINAL_IPAIRS_KEY);
    if (!original.is<sol::protected_function>()) {
        original = lua.globals().raw_get<sol::object>("ipairs");
        if (!original.is<sol::protected_function>()) {
            throw std::runtime_error("Lua ipairs function is not defined");
        }
        registry.raw_set(ORIGINAL_IPAIRS_KEY, original);
    }
    lua_State* state = lua.lua_state();
    original.push();
    lua_pushcclosure(state, nativeIpairs, 1);
    lua_setglobal(state, "ipairs");
}

}  // namespace ludork::standard::container_runtime::detail
