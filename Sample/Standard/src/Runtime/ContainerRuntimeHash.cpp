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

std::uint64_t hashBytes(std::uint64_t seed, const void* data,
                        std::size_t size) {
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    std::uint64_t result = seed;
    for (std::size_t index = 0; index < size; ++index) {
        result ^= bytes[index];
        result *= HASH_PRIME;
    }
    return result;
}

std::uint64_t hashTag(std::uint64_t seed, unsigned char tag) {
    return hashBytes(seed, &tag, sizeof(tag));
}

std::uint64_t hashObject(const sol::object& value);

std::uint64_t hashNumber(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    if (lua_isinteger(state, -1)) {
        const lua_Integer integer = lua_tointeger(state, -1);
        lua_pop(state, 1);
        std::uint64_t result = hashTag(HASH_OFFSET, 3);
        return hashBytes(result, &integer, sizeof(integer));
    }
    const lua_Number number = lua_tonumber(state, -1);
    int integerValid = 0;
    const lua_Integer integer = lua_tointegerx(state, -1, &integerValid);
    lua_pop(state, 1);
    if (!std::isfinite(number)) {
        throw std::invalid_argument(
            "dict numeric keys must be finite and cannot be NaN");
    }
    if (integerValid != 0) {
        std::uint64_t result = hashTag(HASH_OFFSET, 3);
        return hashBytes(result, &integer, sizeof(integer));
    }
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(number));
    std::memcpy(&bits, &number, sizeof(bits));
    std::uint64_t result = hashTag(HASH_OFFSET, 4);
    return hashBytes(result, &bits, sizeof(bits));
}

std::uint64_t hashTuple(const sol::object& value) {
    const NativeTuple& tuple = value.as<NativeTuple&>();
    const sol::table values = sequenceValues(value);
    std::uint64_t result = hashTag(HASH_OFFSET, 7);
    result = hashBytes(result, &tuple.length, sizeof(tuple.length));
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        const std::uint64_t itemHash =
            hashObject(values.raw_get<sol::object>(index));
        result = hashBytes(result, &itemHash, sizeof(itemHash));
    }
    return result;
}

std::uint64_t hashObject(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
            throw std::invalid_argument("dict keys cannot be nil");
        case sol::type::boolean: {
            const bool boolean = value.as<bool>();
            std::uint64_t result = hashTag(HASH_OFFSET, 2);
            return hashBytes(result, &boolean, sizeof(boolean));
        }
        case sol::type::number:
            return hashNumber(value);
        case sol::type::string: {
            const sol::string_view text = value.as<sol::string_view>();
            std::uint64_t result = hashTag(HASH_OFFSET, 5);
            return hashBytes(result, text.data(), text.size());
        }
        case sol::type::userdata:
            if (value.is<NativeTuple>()) {
                return hashTuple(value);
            }
            [[fallthrough]];
        case sol::type::table:
        case sol::type::function:
        case sol::type::thread:
        case sol::type::lightuserdata: {
            const int type = static_cast<int>(value.get_type());
            const void* pointer = objectIdentity(value);
            std::uint64_t result = hashTag(HASH_OFFSET, 8);
            result = hashBytes(result, &type, sizeof(type));
            return hashBytes(result, &pointer, sizeof(pointer));
        }
        default:
            throw std::invalid_argument("unsupported dict key type");
    }
}

bool keyEqual(const sol::object& left, const sol::object& right) {
    if (left.is<NativeTuple>() || right.is<NativeTuple>()) {
        if (!left.is<NativeTuple>() || !right.is<NativeTuple>()) {
            return false;
        }
        const NativeTuple& leftTuple = left.as<NativeTuple&>();
        const NativeTuple& rightTuple = right.as<NativeTuple&>();
        if (leftTuple.length != rightTuple.length) {
            return false;
        }
        const sol::table leftValues = sequenceValues(left);
        const sol::table rightValues = sequenceValues(right);
        for (std::size_t index = 1; index <= leftTuple.length; ++index) {
            if (!keyEqual(leftValues.raw_get<sol::object>(index),
                          rightValues.raw_get<sol::object>(index))) {
                return false;
            }
        }
        return true;
    }
    return rawEqual(left, right);
}

std::size_t findDictEntry(const sol::object& target, const sol::object& key,
                          std::uint64_t hash) {
    const NativeDict& dict = target.as<NativeDict&>();
    const auto bucket = dict.buckets.find(hash);
    if (bucket == dict.buckets.end()) {
        return std::numeric_limits<std::size_t>::max();
    }
    const sol::table keys = dictKeys(target);
    for (const std::size_t index : bucket->second) {
        if (index >= dict.entries.size() || !dict.entries[index].alive) {
            continue;
        }
        if (keyEqual(keys.raw_get<sol::object>(index + 1), key)) {
            return index;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

std::size_t findDictEntry(const sol::object& target, const sol::object& key) {
    return findDictEntry(target, key, hashObject(key));
}

void setDictEntry(sol::state_view lua, const sol::object& target,
                  const sol::object& key, const sol::object& value,
                  bool decodeJsonNull) {
    NativeDict& dict = target.as<NativeDict&>();
    const std::uint64_t hash = hashObject(key);
    const std::size_t existing = findDictEntry(target, key, hash);
    sol::table values = dictValues(target);
    if (existing != std::numeric_limits<std::size_t>::max()) {
        values.raw_set(existing + 1, storedValue(lua, value, decodeJsonNull));
        return;
    }
    const std::size_t index = dict.entries.size();
    dict.entries.push_back({hash, true});
    dict.buckets[hash].push_back(index);
    dictKeys(target).raw_set(index + 1, key);
    values.raw_set(index + 1, storedValue(lua, value, decodeJsonNull));
    ++dict.length;
    ++dict.version;
}

sol::object dictEntryValue(sol::state_view lua, const sol::object& target,
                           std::size_t index) {
    return exposedValue(lua,
                        dictValues(target).raw_get<sol::object>(index + 1));
}

void releaseDictStorage(const sol::object& target) {
    sol::state_view lua(target.lua_state());
    sol::table root = uservalueRoot(target);
    root.raw_set("keys", lua.create_table());
    root.raw_set("values", lua.create_table());
    NativeDict& dict = target.as<NativeDict&>();
    std::vector<NativeDict::Entry>().swap(dict.entries);
    std::unordered_map<std::uint64_t, std::vector<std::size_t>>().swap(
        dict.buckets);
    dict.length = 0;
}

void compactDict(const sol::object& target) {
    NativeDict& dict = target.as<NativeDict&>();
    sol::state_view lua(target.lua_state());
    const sol::table oldKeys = dictKeys(target);
    const sol::table oldValues = dictValues(target);
    sol::table newKeys = lua.create_table();
    sol::table newValues = lua.create_table();
    std::vector<NativeDict::Entry> entries;
    entries.reserve(dict.length);
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> buckets;
    buckets.reserve(dict.length);
    for (std::size_t oldIndex = 0; oldIndex < dict.entries.size(); ++oldIndex) {
        const NativeDict::Entry& entry = dict.entries[oldIndex];
        if (!entry.alive) {
            continue;
        }
        const std::size_t newIndex = entries.size();
        entries.push_back(entry);
        buckets[entry.hash].push_back(newIndex);
        newKeys.raw_set(newIndex + 1,
                        oldKeys.raw_get<sol::object>(oldIndex + 1));
        newValues.raw_set(newIndex + 1,
                          oldValues.raw_get<sol::object>(oldIndex + 1));
    }
    sol::table root = uservalueRoot(target);
    root.raw_set("keys", newKeys);
    root.raw_set("values", newValues);
    dict.entries.swap(entries);
    dict.buckets.swap(buckets);
}

bool removeDictEntry(const sol::object& target, const sol::object& key,
                     sol::object* removedValue) {
    NativeDict& dict = target.as<NativeDict&>();
    const std::size_t index = findDictEntry(target, key);
    if (index == std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    if (removedValue != nullptr) {
        *removedValue =
            dictEntryValue(sol::state_view(target.lua_state()), target, index);
    }
    NativeDict::Entry& entry = dict.entries[index];
    auto bucket = dict.buckets.find(entry.hash);
    if (bucket != dict.buckets.end()) {
        auto storedIndex =
            std::find(bucket->second.begin(), bucket->second.end(), index);
        if (storedIndex != bucket->second.end()) {
            bucket->second.erase(storedIndex);
        }
        if (bucket->second.empty()) {
            dict.buckets.erase(bucket);
        }
    }
    entry.alive = false;
    dictKeys(target).raw_set(index + 1, sol::lua_nil);
    dictValues(target).raw_set(index + 1, sol::lua_nil);
    --dict.length;
    if (dict.length == 0) {
        releaseDictStorage(target);
    } else if (dict.entries.size() > dict.length * 2 + 32) {
        compactDict(target);
    }
    ++dict.version;
    return true;
}

ObjectPair comparisonPair(const sol::object& left, const sol::object& right) {
    const void* leftIdentity = objectIdentity(left);
    const void* rightIdentity = objectIdentity(right);
    return std::less<const void*>{}(leftIdentity, rightIdentity)
               ? ObjectPair{leftIdentity, rightIdentity}
               : ObjectPair{rightIdentity, leftIdentity};
}

bool valueEqual(const sol::object& left, const sol::object& right,
                EqualityContext& context);

bool listEqual(const sol::object& left, const sol::object& right,
               EqualityContext& context) {
    if (rawEqual(left, right)) {
        return true;
    }
    const NativeList& leftList = left.as<NativeList&>();
    const NativeList& rightList = right.as<NativeList&>();
    if (leftList.length != rightList.length) {
        return false;
    }
    if (!context.visited.insert(comparisonPair(left, right)).second) {
        return true;
    }
    const sol::table leftValues = sequenceValues(left);
    const sol::table rightValues = sequenceValues(right);
    for (std::size_t index = 1; index <= leftList.length; ++index) {
        const sol::object leftValue =
            exposedValue(sol::state_view(left.lua_state()),
                         leftValues.raw_get<sol::object>(index));
        const sol::object rightValue =
            exposedValue(sol::state_view(right.lua_state()),
                         rightValues.raw_get<sol::object>(index));
        if (!valueEqual(leftValue, rightValue, context)) {
            return false;
        }
    }
    return true;
}

bool dictEqual(const sol::object& left, const sol::object& right,
               EqualityContext& context) {
    if (rawEqual(left, right)) {
        return true;
    }
    const NativeDict& leftDict = left.as<NativeDict&>();
    const NativeDict& rightDict = right.as<NativeDict&>();
    if (leftDict.length != rightDict.length) {
        return false;
    }
    if (!context.visited.insert(comparisonPair(left, right)).second) {
        return true;
    }
    const sol::table leftKeys = dictKeys(left);
    for (std::size_t index = 0; index < leftDict.entries.size(); ++index) {
        if (!leftDict.entries[index].alive) {
            continue;
        }
        const sol::object key = leftKeys.raw_get<sol::object>(index + 1);
        const std::size_t rightIndex =
            findDictEntry(right, key, leftDict.entries[index].hash);
        if (rightIndex == std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        if (!valueEqual(
                dictEntryValue(sol::state_view(left.lua_state()), left, index),
                dictEntryValue(sol::state_view(right.lua_state()), right,
                               rightIndex),
                context)) {
            return false;
        }
    }
    return true;
}

bool valueEqual(const sol::object& left, const sol::object& right,
                EqualityContext& context) {
    const ContainerKind leftKind = containerKind(left);
    const ContainerKind rightKind = containerKind(right);
    if (leftKind != ContainerKind::None || rightKind != ContainerKind::None) {
        if (leftKind != rightKind) {
            return false;
        }
        switch (leftKind) {
            case ContainerKind::List:
                return listEqual(left, right, context);
            case ContainerKind::Tuple:
                return keyEqual(left, right);
            case ContainerKind::Dict:
                return dictEqual(left, right, context);
            default:
                return false;
        }
    }
    return luaEqual(left, right);
}

bool valueEqual(const sol::object& left, const sol::object& right) {
    EqualityContext context;
    return valueEqual(left, right, context);
}

}  // namespace ludork::standard::container_runtime::detail
