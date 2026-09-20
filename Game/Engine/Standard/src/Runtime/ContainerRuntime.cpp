#include <LuaError.hpp>
#include "ContainerRuntime.hpp"
#include <JsonRuntimeProtocol.hpp>

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

lua_glue::Object nilObject(lua_glue::StateView lua) {
    return lua_glue::MakeObject(lua, lua_glue::nil);
}

lua_glue::Object nilSentinel(lua_glue::StateView lua) {
    return lua_glue::MakeObject(
        lua, lua_glue::LightUserdata(static_cast<void*>(&nilSentinelStorage)));
}

const void* objectIdentity(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
}

bool rawEqual(const lua_glue::Object& left, const lua_glue::Object& right) {
    return left == right;
}

bool luaEqual(const lua_glue::Object& left, const lua_glue::Object& right) {
    lua_State* state = left.lua_state();
    lua_glue::StackGuard stack(state);
    left.push(state);
    right.push(state);
    return ludork::standard::compareLuaValues(state, -2, -1, LUA_OPEQ);
}

ContainerKind containerKind(const lua_glue::Object& value) {
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

bool isStoredNil(const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::LightUserdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push(state);
    const bool result =
        lua_touserdata(state, -1) == static_cast<void*>(&nilSentinelStorage);
    lua_pop(state, 1);
    return result;
}

bool isJsonNull(lua_glue::StateView lua, const lua_glue::Object& value) {
    const lua_glue::Object sentinel = lua.registry().raw_get<lua_glue::Object>(
        ludork::standard::json_runtime::protocol::JSON_NULL_KEY);
    return sentinel.valid() && sentinel.get_type() != lua_glue::Type::Nil &&
           rawEqual(value, sentinel);
}

lua_glue::Object storedValue(lua_glue::StateView lua,
                             const lua_glue::Object& value,
                             bool decodeJsonNull) {
    if (value.get_type() == lua_glue::Type::Nil ||
        (decodeJsonNull && isJsonNull(lua, value))) {
        return nilSentinel(lua);
    }
    return value;
}

lua_glue::Object exposedValue(lua_glue::StateView lua,
                              const lua_glue::Object& value) {
    return isStoredNil(value) ? nilObject(lua) : value;
}

void setUservalueRoot(const lua_glue::Object& value,
                      const lua_glue::Table& root) {
    lua_State* state = value.lua_state();
    value.push(state);
    root.push(state);
    if (lua_setiuservalue(state, -2, 1) == 0) {
        lua_pop(state, 1);
        throw std::runtime_error("Native container has no uservalue slot");
    }
    lua_pop(state, 1);
}

lua_glue::Table uservalueRoot(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
        lua_pop(state, 2);
        throw std::runtime_error("Native container backing table is missing");
    }
    lua_glue::Table result = lua_glue::Read<lua_glue::Table>(state, -1);
    lua_pop(state, 2);
    return result;
}

lua_glue::Table sequenceValues(const lua_glue::Object& value) {
    const lua_glue::Object rawValues =
        uservalueRoot(value).raw_get<lua_glue::Object>("values");
    if (!rawValues.is<lua_glue::Table>()) {
        throw std::runtime_error("Native sequence backing values are missing");
    }
    return rawValues.as<lua_glue::Table>();
}

lua_glue::Table dictKeys(const lua_glue::Object& value) {
    const lua_glue::Object rawKeys =
        uservalueRoot(value).raw_get<lua_glue::Object>("keys");
    if (!rawKeys.is<lua_glue::Table>()) {
        throw std::runtime_error("Native dictionary backing keys are missing");
    }
    return rawKeys.as<lua_glue::Table>();
}

lua_glue::Table dictValues(const lua_glue::Object& value) {
    const lua_glue::Object rawValues =
        uservalueRoot(value).raw_get<lua_glue::Object>("values");
    if (!rawValues.is<lua_glue::Table>()) {
        throw std::runtime_error(
            "Native dictionary backing values are missing");
    }
    return rawValues.as<lua_glue::Table>();
}

lua_glue::Object createList(lua_glue::StateView lua) {
    lua_glue::Object result = lua_glue::MakeObject(lua, NativeList{});
    lua_glue::Table root = lua.create_table();
    root.raw_set("values", lua.create_table());
    setUservalueRoot(result, root);
    return result;
}

lua_glue::Object createTuple(lua_glue::StateView lua) {
    lua_glue::Object result = lua_glue::MakeObject(lua, NativeTuple{});
    lua_glue::Table root = lua.create_table();
    root.raw_set("values", lua.create_table());
    setUservalueRoot(result, root);
    return result;
}

lua_glue::Object createDict(lua_glue::StateView lua) {
    lua_glue::Object result = lua_glue::MakeObject(lua, NativeDict{});
    lua_glue::Table root = lua.create_table();
    root.raw_set("keys", lua.create_table());
    root.raw_set("values", lua.create_table());
    setUservalueRoot(result, root);
    return result;
}

std::size_t rawSequenceLength(const lua_glue::Table& source) {
    const lua_glue::Object rawLength = source.raw_get<lua_glue::Object>("n");
    if (rawLength.is<lua_Integer>()) {
        const lua_Integer length = rawLength.as<lua_Integer>();
        if (length >= 0) {
            return static_cast<std::size_t>(length);
        }
    }
    lua_State* state = source.lua_state();
    source.push(state);
    const std::size_t result = lua_rawlen(state, -1);
    lua_pop(state, 1);
    return result;
}

std::size_t sequenceLength(const lua_glue::Object& source) {
    if (source.is<NativeList>()) {
        return source.as<NativeList&>().length;
    }
    if (source.is<NativeTuple>()) {
        return source.as<NativeTuple&>().length;
    }
    if (source.get_type() == lua_glue::Type::Table) {
        return rawSequenceLength(source.as<lua_glue::Table>());
    }
    throw std::invalid_argument(
        "Sequence source must be a table, list, or tuple");
}

bool isSequenceSource(const lua_glue::Object& source) {
    return source.get_type() == lua_glue::Type::Table ||
           source.is<NativeList>() || source.is<NativeTuple>();
}

lua_glue::Object sequenceItem(lua_glue::StateView lua,
                              const lua_glue::Object& source, std::size_t index,
                              bool decodeRawJsonNull) {
    const bool rawTable = source.get_type() == lua_glue::Type::Table;
    lua_glue::Object value =
        rawTable ? source.as<lua_glue::Table>().raw_get<lua_glue::Object>(index)
                 : sequenceValues(source).raw_get<lua_glue::Object>(index);
    if (!rawTable) {
        return exposedValue(lua, value);
    }
    if (decodeRawJsonNull && isJsonNull(lua, value)) {
        return nilObject(lua);
    }
    return value;
}

void appendListValue(lua_glue::StateView lua, const lua_glue::Object& target,
                     const lua_glue::Object& value, bool decodeJsonNull,
                     bool structuralChange) {
    NativeList& list = target.as<NativeList&>();
    sequenceValues(target).raw_set(list.length + 1,
                                   storedValue(lua, value, decodeJsonNull));
    ++list.length;
    if (structuralChange) {
        ++list.version;
    }
}

void appendTupleValue(lua_glue::StateView lua, const lua_glue::Object& target,
                      const lua_glue::Object& value, bool decodeJsonNull) {
    if (value.get_type() == lua_glue::Type::Nil ||
        (decodeJsonNull && isJsonNull(lua, value))) {
        throw std::invalid_argument("tuple elements cannot be nil");
    }
    NativeTuple& tuple = target.as<NativeTuple&>();
    sequenceValues(target).raw_set(tuple.length + 1, value);
    ++tuple.length;
}

std::vector<lua_glue::Object> constructorValues(lua_glue::StateView lua,
                                                lua_glue::Arguments arguments,
                                                bool& decodedFromRawTable) {
    decodedFromRawTable = false;
    std::vector<lua_glue::Object> result;
    if (arguments.size() == 1) {
        const lua_glue::Object source = arguments.get<lua_glue::Object>();
        if (isSequenceSource(source)) {
            const std::size_t length = sequenceLength(source);
            result.reserve(length);
            decodedFromRawTable = source.get_type() == lua_glue::Type::Table;
            for (std::size_t index = 1; index <= length; ++index) {
                result.push_back(sequenceItem(lua, source, index, true));
            }
            return result;
        }
    }
    result.reserve(arguments.size());
    for (const lua_glue::StackValue& argument : arguments) {
        result.push_back(argument.get<lua_glue::Object>());
    }
    return result;
}

}  // namespace detail

using namespace detail;

void registerContainers(lua_glue::StateView lua) {
    lua_pushcfunction(lua.lua_state(), lessThan);
    lua.registry().raw_set(
        LESS_THAN_KEY, lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1));
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
    const lua_glue::Object value =
        lua_glue::Read<lua_glue::Object>(state, index);
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

bool isContainer(const lua_glue::Object& value) {
    return containerKind(value) != ContainerKind::None;
}

std::size_t containerStorageSize(const lua_glue::Object& value) {
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

std::vector<lua_glue::Object> containerChildren(const lua_glue::Object& value) {
    lua_glue::StateView lua(value.lua_state());
    std::vector<lua_glue::Object> result;
    if (value.is<NativeList>()) {
        const NativeList& list = value.as<NativeList&>();
        result.reserve(list.length);
        const lua_glue::Table values = sequenceValues(value);
        for (std::size_t index = 1; index <= list.length; ++index) {
            result.push_back(
                exposedValue(lua, values.raw_get<lua_glue::Object>(index)));
        }
        return result;
    }
    if (value.is<NativeTuple>()) {
        const NativeTuple& tuple = value.as<NativeTuple&>();
        result.reserve(tuple.length);
        const lua_glue::Table values = sequenceValues(value);
        for (std::size_t index = 1; index <= tuple.length; ++index) {
            result.push_back(values.raw_get<lua_glue::Object>(index));
        }
        return result;
    }
    if (value.is<NativeDict>()) {
        const NativeDict& dict = value.as<NativeDict&>();
        result.reserve(dict.length * 2);
        const lua_glue::Table keys = dictKeys(value);
        for (std::size_t index = 0; index < dict.entries.size(); ++index) {
            if (!dict.entries[index].alive) {
                continue;
            }
            result.push_back(keys.raw_get<lua_glue::Object>(index + 1));
            result.push_back(dictEntryValue(lua, value, index));
        }
    }
    return result;
}

}  // namespace ludork::standard::container_runtime
