#include "ContainerRuntime.hpp"

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

namespace ludork::standard::container_runtime {

namespace detail {

unsigned char nilSentinelStorage;

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

sol::object nilSentinel(sol::state_view lua) {
    return sol::make_object(
        lua, sol::lightuserdata_value(static_cast<void*>(&nilSentinelStorage)));
}

const void* objectIdentity(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
}

bool rawEqual(const sol::object& left, const sol::object& right) {
    lua_State* state = left.lua_state();
    left.push();
    right.push();
    const bool result = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    return result;
}

bool luaEqual(const sol::object& left, const sol::object& right) {
    lua_State* state = left.lua_state();
    left.push();
    right.push();
    const bool result = lua_compare(state, -2, -1, LUA_OPEQ) != 0;
    lua_pop(state, 2);
    return result;
}

ContainerKind containerKind(const sol::object& value) {
    if (value.is<NativeList>()) {
        return ContainerKind::List;
    }
    if (value.is<NativeTuple>()) {
        return ContainerKind::Tuple;
    }
    if (value.is<NativeDict>()) {
        return ContainerKind::Dict;
    }
    return ContainerKind::None;
}

bool isStoredNil(const sol::object& value) {
    if (value.get_type() != sol::type::lightuserdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    const bool result =
        lua_touserdata(state, -1) == static_cast<void*>(&nilSentinelStorage);
    lua_pop(state, 1);
    return result;
}

bool isJsonNull(sol::state_view lua, const sol::object& value) {
    const sol::object sentinel =
        lua.registry().raw_get<sol::object>(JSON_NULL_KEY);
    return sentinel.valid() && sentinel.get_type() != sol::type::lua_nil &&
           rawEqual(value, sentinel);
}

sol::object storedValue(sol::state_view lua, const sol::object& value,
                        bool decodeJsonNull) {
    if (value.get_type() == sol::type::lua_nil ||
        (decodeJsonNull && isJsonNull(lua, value))) {
        return nilSentinel(lua);
    }
    return value;
}

sol::object exposedValue(sol::state_view lua, const sol::object& value) {
    return isStoredNil(value) ? nilObject(lua) : value;
}

void setUservalueRoot(const sol::object& value, const sol::table& root) {
    lua_State* state = value.lua_state();
    value.push();
    root.push();
    if (lua_setiuservalue(state, -2, 1) == 0) {
        lua_pop(state, 1);
        throw std::runtime_error("Native container has no uservalue slot");
    }
    lua_pop(state, 1);
}

sol::table uservalueRoot(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
        lua_pop(state, 2);
        throw std::runtime_error("Native container backing table is missing");
    }
    sol::table result = sol::stack::get<sol::table>(state, -1);
    lua_pop(state, 2);
    return result;
}

sol::table sequenceValues(const sol::object& value) {
    const sol::object rawValues =
        uservalueRoot(value).raw_get<sol::object>("values");
    if (!rawValues.is<sol::table>()) {
        throw std::runtime_error("Native sequence backing values are missing");
    }
    return rawValues.as<sol::table>();
}

sol::table dictKeys(const sol::object& value) {
    const sol::object rawKeys =
        uservalueRoot(value).raw_get<sol::object>("keys");
    if (!rawKeys.is<sol::table>()) {
        throw std::runtime_error("Native dictionary backing keys are missing");
    }
    return rawKeys.as<sol::table>();
}

sol::table dictValues(const sol::object& value) {
    const sol::object rawValues =
        uservalueRoot(value).raw_get<sol::object>("values");
    if (!rawValues.is<sol::table>()) {
        throw std::runtime_error(
            "Native dictionary backing values are missing");
    }
    return rawValues.as<sol::table>();
}

sol::object createList(sol::state_view lua) {
    sol::object result = sol::make_object(lua, NativeList{});
    sol::table root = lua.create_table();
    root.raw_set("values", lua.create_table());
    setUservalueRoot(result, root);
    return result;
}

sol::object createTuple(sol::state_view lua) {
    sol::object result = sol::make_object(lua, NativeTuple{});
    sol::table root = lua.create_table();
    root.raw_set("values", lua.create_table());
    setUservalueRoot(result, root);
    return result;
}

sol::object createDict(sol::state_view lua) {
    sol::object result = sol::make_object(lua, NativeDict{});
    sol::table root = lua.create_table();
    root.raw_set("keys", lua.create_table());
    root.raw_set("values", lua.create_table());
    setUservalueRoot(result, root);
    return result;
}

std::size_t rawSequenceLength(const sol::table& source) {
    const sol::object rawLength = source.raw_get<sol::object>("n");
    if (rawLength.is<lua_Integer>()) {
        const lua_Integer length = rawLength.as<lua_Integer>();
        if (length >= 0) {
            return static_cast<std::size_t>(length);
        }
    }
    lua_State* state = source.lua_state();
    source.push();
    const std::size_t result = lua_rawlen(state, -1);
    lua_pop(state, 1);
    return result;
}

std::size_t sequenceLength(const sol::object& source) {
    if (source.is<NativeList>()) {
        return source.as<NativeList&>().length;
    }
    if (source.is<NativeTuple>()) {
        return source.as<NativeTuple&>().length;
    }
    if (source.get_type() == sol::type::table) {
        return rawSequenceLength(source.as<sol::table>());
    }
    throw std::invalid_argument(
        "Sequence source must be a table, list, or tuple");
}

bool isSequenceSource(const sol::object& source) {
    return source.get_type() == sol::type::table || source.is<NativeList>() ||
           source.is<NativeTuple>();
}

sol::object sequenceItem(sol::state_view lua, const sol::object& source,
                         std::size_t index, bool decodeRawJsonNull) {
    const bool rawTable = source.get_type() == sol::type::table;
    sol::object value =
        rawTable ? source.as<sol::table>().raw_get<sol::object>(index)
                 : sequenceValues(source).raw_get<sol::object>(index);
    if (!rawTable) {
        return exposedValue(lua, value);
    }
    if (decodeRawJsonNull && isJsonNull(lua, value)) {
        return nilObject(lua);
    }
    return value;
}

void appendListValue(sol::state_view lua, const sol::object& target,
                     const sol::object& value, bool decodeJsonNull,
                     bool structuralChange) {
    NativeList& list = target.as<NativeList&>();
    sequenceValues(target).raw_set(list.length + 1,
                                   storedValue(lua, value, decodeJsonNull));
    ++list.length;
    if (structuralChange) {
        ++list.version;
    }
}

void appendTupleValue(sol::state_view lua, const sol::object& target,
                      const sol::object& value, bool decodeJsonNull) {
    if (value.get_type() == sol::type::lua_nil ||
        (decodeJsonNull && isJsonNull(lua, value))) {
        throw std::invalid_argument("tuple elements cannot be nil");
    }
    NativeTuple& tuple = target.as<NativeTuple&>();
    sequenceValues(target).raw_set(tuple.length + 1, value);
    ++tuple.length;
}

std::vector<sol::object> constructorValues(sol::state_view lua,
                                           sol::variadic_args arguments,
                                           bool& decodedFromRawTable) {
    decodedFromRawTable = false;
    std::vector<sol::object> result;
    if (arguments.size() == 1) {
        const sol::object source = arguments.begin()->get<sol::object>();
        if (isSequenceSource(source)) {
            const std::size_t length = sequenceLength(source);
            result.reserve(length);
            decodedFromRawTable = source.get_type() == sol::type::table;
            for (std::size_t index = 1; index <= length; ++index) {
                result.push_back(sequenceItem(lua, source, index, true));
            }
            return result;
        }
    }
    result.reserve(arguments.size());
    for (const sol::stack_proxy& argument : arguments) {
        result.push_back(argument.get<sol::object>());
    }
    return result;
}

}  // namespace detail

using namespace detail;

void registerContainers(sol::state_view lua) {
    lua_pushcfunction(lua.lua_state(), lessThan);
    lua.registry().raw_set(LESS_THAN_KEY,
                           sol::stack::get<sol::object>(lua.lua_state(), -1));
    lua_pop(lua.lua_state(), 1);
    registerList(lua);
    registerTuple(lua);
    registerDict(lua);
    registerIpairs(lua);
}

void shutdownContainers(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    const int stackTop = lua_gettop(state);
    lua_getfield(state, LUA_REGISTRYINDEX, ORIGINAL_IPAIRS_KEY);
    if (lua_isfunction(state, -1)) {
        lua_setglobal(state, "ipairs");
    } else {
        lua_pop(state, 1);
    }
    constexpr const char* globals[] = {"list", "tuple", "dict"};
    for (const char* name : globals) {
        lua_pushnil(state);
        lua_setglobal(state, name);
    }
    constexpr const char* registryKeys[] = {ORIGINAL_IPAIRS_KEY, LESS_THAN_KEY};
    for (const char* key : registryKeys) {
        lua_pushnil(state);
        lua_setfield(state, LUA_REGISTRYINDEX, key);
    }
    lua_settop(state, stackTop);
}

bool containerLength(lua_State* state, int index, std::size_t& length) {
    const sol::object value = sol::stack::get<sol::object>(state, index);
    if (value.is<NativeList>()) {
        length = value.as<NativeList&>().length;
        return true;
    }
    if (value.is<NativeTuple>()) {
        length = value.as<NativeTuple&>().length;
        return true;
    }
    if (value.is<NativeDict>()) {
        length = value.as<NativeDict&>().length;
        return true;
    }
    return false;
}

bool isContainer(const sol::object& value) {
    return containerKind(value) != ContainerKind::None;
}

std::size_t containerStorageSize(const sol::object& value) {
    if (value.is<NativeList>()) {
        const NativeList& list = value.as<NativeList&>();
        return sizeof(NativeList) + list.length * sizeof(void*) * 2;
    }
    if (value.is<NativeTuple>()) {
        const NativeTuple& tuple = value.as<NativeTuple&>();
        return sizeof(NativeTuple) + tuple.length * sizeof(void*) * 2;
    }
    if (value.is<NativeDict>()) {
        const NativeDict& dict = value.as<NativeDict&>();
        std::size_t result =
            sizeof(NativeDict) +
            dict.entries.capacity() * sizeof(NativeDict::Entry) +
            dict.buckets.bucket_count() * sizeof(void*) * 2;
        for (const auto& bucket : dict.buckets) {
            result += bucket.second.capacity() * sizeof(std::size_t);
        }
        return result + dict.length * sizeof(void*) * 4;
    }
    return 0;
}

std::vector<sol::object> containerChildren(const sol::object& value) {
    sol::state_view lua(value.lua_state());
    std::vector<sol::object> result;
    if (value.is<NativeList>()) {
        const NativeList& list = value.as<NativeList&>();
        result.reserve(list.length);
        const sol::table values = sequenceValues(value);
        for (std::size_t index = 1; index <= list.length; ++index) {
            result.push_back(
                exposedValue(lua, values.raw_get<sol::object>(index)));
        }
        return result;
    }
    if (value.is<NativeTuple>()) {
        const NativeTuple& tuple = value.as<NativeTuple&>();
        result.reserve(tuple.length);
        const sol::table values = sequenceValues(value);
        for (std::size_t index = 1; index <= tuple.length; ++index) {
            result.push_back(values.raw_get<sol::object>(index));
        }
        return result;
    }
    if (value.is<NativeDict>()) {
        const NativeDict& dict = value.as<NativeDict&>();
        result.reserve(dict.length * 2);
        const sol::table keys = dictKeys(value);
        for (std::size_t index = 0; index < dict.entries.size(); ++index) {
            if (!dict.entries[index].alive) {
                continue;
            }
            result.push_back(keys.raw_get<sol::object>(index + 1));
            result.push_back(dictEntryValue(lua, value, index));
        }
    }
    return result;
}

}  // namespace ludork::standard::container_runtime
