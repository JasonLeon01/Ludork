#include <LuaError.hpp>
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

lua_Integer checkedIndex(const lua_glue::Object& value, const char* name) {
    if (value.get_type() != lua_glue::Type::Number) {
        throw std::invalid_argument(std::string(name) + " must be an integer");
    }
    lua_State* state = value.lua_state();
    value.push(state);
    int valid = 0;
    const lua_Integer result = lua_tointegerx(state, -1, &valid);
    lua_pop(state, 1);
    if (valid == 0) {
        throw std::invalid_argument(std::string(name) + " must be an integer");
    }
    return result;
}

lua_glue::Object typeMember(lua_glue::StateView lua, const char* typeName,
                            const lua_glue::Object& key) {
    const lua_glue::Object rawType =
        lua.globals().raw_get<lua_glue::Object>(typeName);
    if (!rawType.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    const lua_glue::Object result =
        rawType.as<lua_glue::Table>().get<lua_glue::Object>(key);
    return result.valid() ? result : nilObject(lua);
}

lua_glue::Object sequenceIndex(lua_glue::ThisState state,
                               const lua_glue::Object& self,
                               const lua_glue::Object& key,
                               const char* typeName, std::size_t length) {
    lua_glue::StateView lua(state);
    if (key.get_type() == lua_glue::Type::Number) {
        const lua_Integer index = checkedIndex(key, "sequence index");
        if (index < 1 || static_cast<std::size_t>(index) > length) {
            return nilObject(lua);
        }
        return exposedValue(
            lua, sequenceValues(self).raw_get<lua_glue::Object>(index));
    }
    const lua_glue::Object member = typeMember(lua, typeName, key);
    if (member.valid() && member.get_type() != lua_glue::Type::Nil) {
        return member;
    }
    return nilObject(lua);
}

int lessThan(lua_State* state) {
    lua_pushboolean(state, lua_compare(state, 1, 2, LUA_OPLT));
    return 1;
}

bool callComparator(const lua_glue::Function& comparator,
                    const lua_glue::Object& left,
                    const lua_glue::Object& right) {
    lua_glue::CallResult result = comparator(left, right);
    if (!result.valid()) {
        const std::string error = result.error();
        throw std::runtime_error(error.c_str());
    }
    if (result.get_type() != lua_glue::Type::Boolean) {
        throw std::invalid_argument("list sort comparator must return boolean");
    }
    return result.get<bool>();
}

int sequenceIteratorBody(lua_State* state) {
    lua_glue::StateView lua(state);
    const lua_glue::Object self =
        lua_glue::Read<lua_glue::Object>(state, lua_upvalueindex(1));
    const lua_Integer expectedVersion =
        lua_tointeger(state, lua_upvalueindex(2));
    lua_Integer cursor = lua_tointeger(state, lua_upvalueindex(3));
    std::size_t length = 0;
    if (self.is<NativeList>()) {
        const NativeList& list = self.as<NativeList&>();
        if (list.version != static_cast<std::uint64_t>(expectedVersion)) {
            throw std::invalid_argument(
                "list changed structure during iteration");
        }
        length = list.length;
    } else if (self.is<NativeTuple>()) {
        length = self.as<NativeTuple&>().length;
    } else {
        throw std::invalid_argument("sequence iterator target is invalid");
    }
    ++cursor;
    if (cursor < 1 || static_cast<std::size_t>(cursor) > length) {
        return 0;
    }
    lua_pushinteger(state, cursor);
    lua_replace(state, lua_upvalueindex(3));
    lua_pushinteger(state, cursor);
    exposedValue(lua, sequenceValues(self).raw_get<lua_glue::Object>(cursor))
        .push(state);
    return 2;
}

int sequenceIterator(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        return sequenceIteratorBody(state);
    });
}

int dictIteratorBody(lua_State* state) {
    lua_glue::StateView lua(state);
    const lua_glue::Object self =
        lua_glue::Read<lua_glue::Object>(state, lua_upvalueindex(1));
    NativeDict& dict = self.as<NativeDict&>();
    const lua_Integer expectedVersion =
        lua_tointeger(state, lua_upvalueindex(2));
    if (dict.version != static_cast<std::uint64_t>(expectedVersion)) {
        throw std::invalid_argument("dict changed structure during iteration");
    }
    lua_Integer cursor = lua_tointeger(state, lua_upvalueindex(3));
    while (++cursor <= static_cast<lua_Integer>(dict.entries.size())) {
        if (!dict.entries[static_cast<std::size_t>(cursor - 1)].alive) {
            continue;
        }
        lua_pushinteger(state, cursor);
        lua_replace(state, lua_upvalueindex(3));
        dictKeys(self).raw_get<lua_glue::Object>(cursor).push(state);
        dictEntryValue(lua, self, static_cast<std::size_t>(cursor - 1))
            .push(state);
        return 2;
    }
    return 0;
}

int dictIterator(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        return dictIteratorBody(state);
    });
}

void pushSequenceIterator(lua_State* state, int selfIndex) {
    const int absoluteIndex = lua_absindex(state, selfIndex);
    const lua_glue::Object self =
        lua_glue::Read<lua_glue::Object>(state, absoluteIndex);
    const std::uint64_t version =
        self.is<NativeList>() ? self.as<NativeList&>().version : 0;
    lua_pushvalue(state, absoluteIndex);
    lua_pushinteger(state, static_cast<lua_Integer>(version));
    lua_pushinteger(state, 0);
    lua_pushcclosure(state, sequenceIterator, 3);
}

void pushDictIterator(lua_State* state, int selfIndex) {
    const int absoluteIndex = lua_absindex(state, selfIndex);
    const lua_glue::Object self =
        lua_glue::Read<lua_glue::Object>(state, absoluteIndex);
    lua_pushvalue(state, absoluteIndex);
    lua_pushinteger(state,
                    static_cast<lua_Integer>(self.as<NativeDict&>().version));
    lua_pushinteger(state, 0);
    lua_pushcclosure(state, dictIterator, 3);
}

int nativeIpairsBody(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    const lua_glue::Object target = lua_glue::Read<lua_glue::Object>(state, 1);
    if (target.is<NativeList>() || target.is<NativeTuple>()) {
        pushSequenceIterator(state, 1);
        lua_pushnil(state);
        lua_pushnil(state);
        return 3;
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushvalue(state, 1);
    if (ludork::standard::protectedLuaCall(state, 1, LUA_MULTRET) != LUA_OK) {
        throw std::runtime_error(ludork::standard::luaErrorMessage(state, -1));
    }
    return lua_gettop(state) - argumentCount;
}

int nativeIpairs(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        return nativeIpairsBody(state);
    });
}

int typeTableIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
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
            if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
                throw std::runtime_error(
                    ludork::standard::luaErrorMessage(state, -1));
            }
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
    });
}

void maskNewConstructor(lua_glue::Table typeTable) {
    lua_State* state = typeTable.lua_state();
    typeTable.push(state);
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        throw std::runtime_error("Native container type metatable is missing");
    }
    lua_getfield(state, -1, "__index");
    lua_pushcclosure(state, typeTableIndex, 1);
    lua_setfield(state, -2, "__index");
    lua_pop(state, 2);
}

lua_glue::MultipleResults sequencePairs(const lua_glue::Object& self,
                                        lua_glue::ThisState state) {
    lua_State* luaState = state;
    self.push(luaState);
    pushSequenceIterator(luaState, -1);
    lua_glue::Function iterator =
        lua_glue::Read<lua_glue::Function>(luaState, -1);
    lua_pop(luaState, 2);
    lua_glue::StateView lua(luaState);
    return {iterator, nilObject(lua), nilObject(lua)};
}

lua_glue::MultipleResults nativeDictPairs(const lua_glue::Object& self,
                                          lua_glue::ThisState state) {
    lua_State* luaState = state;
    self.push(luaState);
    pushDictIterator(luaState, -1);
    lua_glue::Function iterator =
        lua_glue::Read<lua_glue::Function>(luaState, -1);
    lua_pop(luaState, 2);
    lua_glue::StateView lua(luaState);
    return {iterator, nilObject(lua), nilObject(lua)};
}

void overrideNewIndex(const lua_glue::Object& sample, lua_CFunction newIndex) {
    lua_State* state = sample.lua_state();
    sample.push(state);
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        throw std::runtime_error(
            "Native container instance metatable is missing");
    }
    lua_pushcfunction(state, newIndex);
    lua_setfield(state, -2, "__newindex");
    lua_pop(state, 2);
}

void registerIpairs(lua_glue::StateView lua) {
    lua_glue::Table registry = lua.registry();
    lua_glue::Object original =
        registry.raw_get<lua_glue::Object>(ORIGINAL_IPAIRS_KEY);
    if (!original.is<lua_glue::Function>()) {
        original = lua.globals().raw_get<lua_glue::Object>("ipairs");
        if (!original.is<lua_glue::Function>()) {
            throw std::runtime_error("Lua ipairs function is not defined");
        }
        registry.raw_set(ORIGINAL_IPAIRS_KEY, original);
    }
    lua_State* state = lua.lua_state();
    original.push(state);
    lua_pushcclosure(state, nativeIpairs, 1);
    lua_setglobal(state, "ipairs");
}

}  // namespace ludork::standard::container_runtime::detail
