#include "ContainerRuntime.hpp"

#include "ClassRuntime.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
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

namespace {

constexpr const char* ORIGINAL_IPAIRS_KEY =
    "LudorkStandard.ContainerOriginalIpairs";
constexpr const char* LESS_THAN_KEY = "LudorkStandard.ContainerLessThan";
constexpr const char* JSON_NULL_KEY = "LuaSF.JsonNullSentinel";
constexpr const char* JSON_ARRAY_METATABLE_KEY = "LuaSF.JsonArrayMetatable";
constexpr const char* JSON_EMPTY_ARRAY_METATABLE_KEY =
    "LuaSF.JsonEmptyArrayMetatable";
constexpr std::uint64_t HASH_OFFSET = 1469598103934665603ULL;
constexpr std::uint64_t HASH_PRIME = 1099511628211ULL;

unsigned char nilSentinelStorage;

struct NativeList {
    std::size_t length = 0;
    std::uint64_t version = 0;
};

struct NativeTuple {
    std::size_t length = 0;
};

struct NativeDict {
    struct Entry {
        std::uint64_t hash = 0;
        bool alive = false;
    };

    std::vector<Entry> entries;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> buckets;
    std::size_t length = 0;
    std::uint64_t version = 0;
};

enum class ContainerKind {
    None,
    List,
    Tuple,
    Dict,
};

struct ObjectPair {
    const void* first = nullptr;
    const void* second = nullptr;

    bool operator==(const ObjectPair& other) const noexcept {
        return first == other.first && second == other.second;
    }
};

struct ObjectPairHash {
    std::size_t operator()(const ObjectPair& value) const noexcept {
        std::size_t first = std::hash<const void*>{}(value.first);
        std::size_t second = std::hash<const void*>{}(value.second);
        return first ^ (second + 0x9e3779b9U + (first << 6U) + (first >> 2U));
    }
};

struct EqualityContext {
    std::unordered_set<ObjectPair, ObjectPairHash> visited;
};

struct TableConversionContext {
    explicit TableConversionContext(sol::state_view state) : lua(state) {}

    sol::state_view lua;
    std::unordered_map<const void*, sol::table> converted;
};

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
                     bool structuralChange = true) {
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
    const sol::object member = typeMember(lua, typeName, key);
    if (member.valid() && member.get_type() != sol::type::lua_nil) {
        return member;
    }
    if (key.get_type() != sol::type::number) {
        return nilObject(lua);
    }
    const lua_Integer index = checkedIndex(key, "sequence index");
    if (index < 1 || static_cast<std::size_t>(index) > length) {
        return nilObject(lua);
    }
    return exposedValue(lua, sequenceValues(self).raw_get<sol::object>(index));
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

auto sequencePairs(const sol::object& self, sol::this_state state) {
    lua_State* luaState = state;
    self.push();
    pushSequenceIterator(luaState, -1);
    sol::function iterator = sol::stack::get<sol::function>(luaState, -1);
    lua_pop(luaState, 2);
    sol::state_view lua(luaState);
    return std::make_tuple(iterator, nilObject(lua), nilObject(lua));
}

auto nativeDictPairs(const sol::object& self, sol::this_state state) {
    lua_State* luaState = state;
    self.push();
    pushDictIterator(luaState, -1);
    sol::function iterator = sol::stack::get<sol::function>(luaState, -1);
    lua_pop(luaState, 2);
    sol::state_view lua(luaState);
    return std::make_tuple(iterator, nilObject(lua), nilObject(lua));
}

void setListIndex(const sol::object& self, const sol::object& key,
                  const sol::object& value, sol::this_state state) {
    const lua_Integer index = checkedIndex(key, "list index");
    NativeList& list = self.as<NativeList&>();
    if (index < 1 || static_cast<std::size_t>(index) > list.length + 1) {
        throw std::out_of_range("list index assignment is out of range");
    }
    sol::state_view lua(state);
    if (static_cast<std::size_t>(index) == list.length + 1) {
        appendListValue(lua, self, value, false);
        return;
    }
    sequenceValues(self).raw_set(index, storedValue(lua, value, false));
}

void rejectTupleWrite(const sol::object&, const sol::object&,
                      const sol::object&) {
    throw std::invalid_argument("tuple is immutable");
}

int rawListNewIndex(lua_State* state) {
    try {
        setListIndex(sol::stack::get<sol::object>(state, 1),
                     sol::stack::get<sol::object>(state, 2),
                     sol::stack::get<sol::object>(state, 3),
                     sol::this_state(state));
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int rawTupleNewIndex(lua_State* state) {
    return luaL_error(state, "tuple is immutable");
}

int rawDictNewIndex(lua_State* state) {
    try {
        const sol::object self = sol::stack::get<sol::object>(state, 1);
        setDictEntry(sol::state_view(state), self,
                     sol::stack::get<sol::object>(state, 2),
                     sol::stack::get<sol::object>(state, 3), false);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
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

void listAppend(const sol::object& self, sol::variadic_args arguments,
                sol::this_state state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.append expects one value");
    }
    appendListValue(sol::state_view(state), self, arguments.get<sol::object>(),
                    false);
}

void listExtend(const sol::object& self, const sol::object& source,
                sol::this_state state) {
    if (!isSequenceSource(source)) {
        throw std::invalid_argument(
            "list.extend source must be a table, list, or tuple");
    }
    sol::state_view lua(state);
    const std::size_t length = sequenceLength(source);
    std::vector<sol::object> values;
    values.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        values.push_back(sequenceItem(lua, source, index, true));
    }
    const bool decodeJsonNull = source.get_type() == sol::type::table;
    for (const sol::object& value : values) {
        appendListValue(lua, self, value, decodeJsonNull);
    }
}

void listInsert(const sol::object& self, const sol::object& rawIndex,
                sol::variadic_args arguments, sol::this_state state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.insert expects an index and value");
    }
    const lua_Integer index = checkedIndex(rawIndex, "list insert index");
    NativeList& list = self.as<NativeList&>();
    if (index < 1 || static_cast<std::size_t>(index) > list.length + 1) {
        throw std::out_of_range("list insert index is out of range");
    }
    sol::state_view lua(state);
    sol::table values = sequenceValues(self);
    for (std::size_t target = list.length + 1;
         target > static_cast<std::size_t>(index); --target) {
        values.raw_set(target, values.raw_get<sol::object>(target - 1));
    }
    values.raw_set(index,
                   storedValue(lua, arguments.get<sol::object>(), false));
    ++list.length;
    ++list.version;
}

sol::object listPop(const sol::object& self, sol::variadic_args arguments,
                    sol::this_state state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument("list.pop expects at most one index");
    }
    NativeList& list = self.as<NativeList&>();
    if (list.length == 0) {
        throw std::out_of_range("cannot pop from an empty list");
    }
    const lua_Integer index =
        arguments.size() == 0
            ? static_cast<lua_Integer>(list.length)
            : checkedIndex(arguments.get<sol::object>(), "list pop index");
    if (index < 1 || static_cast<std::size_t>(index) > list.length) {
        throw std::out_of_range("list pop index is out of range");
    }
    sol::state_view lua(state);
    sol::table values = sequenceValues(self);
    sol::object result = exposedValue(lua, values.raw_get<sol::object>(index));
    for (std::size_t target = static_cast<std::size_t>(index);
         target < list.length; ++target) {
        values.raw_set(target, values.raw_get<sol::object>(target + 1));
    }
    values.raw_set(list.length, sol::lua_nil);
    --list.length;
    if (list.length == 0) {
        uservalueRoot(self).raw_set("values", lua.create_table());
    }
    ++list.version;
    return result;
}

std::size_t findSequenceValue(const sol::object& self,
                              const sol::object& expected, std::size_t length) {
    const sol::table values = sequenceValues(self);
    sol::state_view lua(self.lua_state());
    for (std::size_t index = 1; index <= length; ++index) {
        if (valueEqual(exposedValue(lua, values.raw_get<sol::object>(index)),
                       expected)) {
            return index;
        }
    }
    return 0;
}

void listRemove(const sol::object& self, sol::variadic_args arguments,
                sol::this_state state) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("list.remove expects one value");
    }
    const std::size_t index = findSequenceValue(
        self, arguments.get<sol::object>(), self.as<NativeList&>().length);
    if (index == 0) {
        throw std::invalid_argument("list.remove value was not found");
    }
    sol::state_view lua(state);
    sol::table values = sequenceValues(self);
    NativeList& list = self.as<NativeList&>();
    for (std::size_t target = index; target < list.length; ++target) {
        values.raw_set(target, values.raw_get<sol::object>(target + 1));
    }
    values.raw_set(list.length, sol::lua_nil);
    --list.length;
    if (list.length == 0) {
        uservalueRoot(self).raw_set("values", lua.create_table());
    }
    ++list.version;
}

void listClear(const sol::object& self) {
    NativeList& list = self.as<NativeList&>();
    const bool changed = list.length != 0;
    uservalueRoot(self).raw_set(
        "values", sol::state_view(self.lua_state()).create_table());
    list.length = 0;
    if (changed) {
        ++list.version;
    }
}

lua_Integer sequenceIndexOf(const sol::object& self,
                            sol::variadic_args arguments, std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.index expects one value");
    }
    const std::size_t index =
        findSequenceValue(self, arguments.get<sol::object>(), length);
    if (index == 0) {
        throw std::invalid_argument("sequence.index value was not found");
    }
    return static_cast<lua_Integer>(index);
}

lua_Integer sequenceCount(const sol::object& self, sol::variadic_args arguments,
                          std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.count expects one value");
    }
    const sol::object expected = arguments.get<sol::object>();
    const sol::table values = sequenceValues(self);
    sol::state_view lua(self.lua_state());
    lua_Integer result = 0;
    for (std::size_t index = 1; index <= length; ++index) {
        if (valueEqual(exposedValue(lua, values.raw_get<sol::object>(index)),
                       expected)) {
            ++result;
        }
    }
    return result;
}

bool sequenceContains(const sol::object& self, sol::variadic_args arguments,
                      std::size_t length) {
    if (arguments.size() != 1) {
        throw std::invalid_argument("sequence.contains expects one value");
    }
    return findSequenceValue(self, arguments.get<sol::object>(), length) != 0;
}

void listReverse(const sol::object& self) {
    NativeList& list = self.as<NativeList&>();
    if (list.length < 2) {
        return;
    }
    sol::table values = sequenceValues(self);
    for (std::size_t left = 1, right = list.length; left < right;
         ++left, --right) {
        const sol::object leftValue = values.raw_get<sol::object>(left);
        values.raw_set(left, values.raw_get<sol::object>(right));
        values.raw_set(right, leftValue);
    }
    ++list.version;
}

void listSort(const sol::object& self, sol::variadic_args arguments,
              sol::this_state state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument("list.sort expects at most one comparator");
    }
    sol::state_view lua(state);
    sol::protected_function comparator;
    if (arguments.size() == 0 || arguments.get_type() == sol::type::lua_nil) {
        const sol::object rawComparator =
            lua.registry().raw_get<sol::object>(LESS_THAN_KEY);
        if (!rawComparator.is<sol::protected_function>()) {
            throw std::runtime_error("list default comparator is missing");
        }
        comparator = rawComparator.as<sol::protected_function>();
    } else {
        const sol::object rawComparator = arguments.get<sol::object>();
        if (!rawComparator.is<sol::protected_function>()) {
            throw std::invalid_argument(
                "list sort comparator must be a function");
        }
        comparator = rawComparator.as<sol::protected_function>();
    }
    NativeList& list = self.as<NativeList&>();
    const std::uint64_t version = list.version;
    sol::table backing = sequenceValues(self);
    std::vector<sol::object> values;
    values.reserve(list.length);
    for (std::size_t index = 1; index <= list.length; ++index) {
        values.push_back(
            exposedValue(lua, backing.raw_get<sol::object>(index)));
    }
    std::stable_sort(
        values.begin(), values.end(),
        [&comparator](const sol::object& left, const sol::object& right) {
            return callComparator(comparator, left, right);
        });
    if (list.version != version) {
        throw std::runtime_error(
            "list changed structure inside sort comparator");
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
        backing.raw_set(index + 1, storedValue(lua, values[index], false));
    }
    if (list.length > 1) {
        ++list.version;
    }
}

auto sequenceUnpack(const sol::object& self) {
    const std::size_t length = self.is<NativeList>()
                                   ? self.as<NativeList&>().length
                                   : self.as<NativeTuple&>().length;
    sol::state_view lua(self.lua_state());
    const sol::table values = sequenceValues(self);
    std::vector<sol::object> result;
    result.reserve(length);
    for (std::size_t index = 1; index <= length; ++index) {
        result.push_back(exposedValue(lua, values.raw_get<sol::object>(index)));
    }
    return sol::as_returns(std::move(result));
}

sol::object copyList(const sol::object& source, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeList& sourceList = source.as<NativeList&>();
    const sol::table values = sequenceValues(source);
    for (std::size_t index = 1; index <= sourceList.length; ++index) {
        appendListValue(lua, result,
                        exposedValue(lua, values.raw_get<sol::object>(index)),
                        false, false);
    }
    return result;
}

std::size_t nativeSequenceLength(const sol::object& value,
                                 ContainerKind kind) {
    if (kind == ContainerKind::List) {
        if (!value.is<NativeList>()) {
            throw std::invalid_argument("list operand expected");
        }
        return value.as<NativeList&>().length;
    }
    if (!value.is<NativeTuple>()) {
        throw std::invalid_argument("tuple operand expected");
    }
    return value.as<NativeTuple&>().length;
}

sol::object createNativeSequence(sol::state_view lua, ContainerKind kind) {
    return kind == ContainerKind::List ? createList(lua) : createTuple(lua);
}

void appendNativeSequenceValue(sol::state_view lua, const sol::object& target,
                               const sol::object& value,
                               ContainerKind kind) {
    if (kind == ContainerKind::List) {
        appendListValue(lua, target, value, false, false);
        return;
    }
    appendTupleValue(lua, target, value, false);
}

void appendNativeSequence(sol::state_view lua, const sol::object& target,
                          const sol::object& source, ContainerKind kind) {
    const std::size_t length = nativeSequenceLength(source, kind);
    for (std::size_t index = 1; index <= length; ++index) {
        appendNativeSequenceValue(
            lua, target, sequenceItem(lua, source, index, false), kind);
    }
}

sol::object concatenateNativeSequences(const sol::object& left,
                                       const sol::object& right,
                                       ContainerKind kind,
                                       sol::this_state state) {
    const std::size_t leftLength = nativeSequenceLength(left, kind);
    const std::size_t rightLength = nativeSequenceLength(right, kind);
    if (rightLength > std::numeric_limits<std::size_t>::max() - leftLength) {
        throw std::length_error("concatenated sequence is too large");
    }
    sol::state_view lua(state);
    sol::object result = createNativeSequence(lua, kind);
    appendNativeSequence(lua, result, left, kind);
    appendNativeSequence(lua, result, right, kind);
    return result;
}

sol::object repeatNativeSequence(const sol::object& left,
                                 const sol::object& right,
                                 ContainerKind kind,
                                 sol::this_state state) {
    const bool leftSequence = containerKind(left) == kind;
    const sol::object& source = leftSequence ? left : right;
    const sol::object& rawRepetitions = leftSequence ? right : left;
    if (!rawRepetitions.is<lua_Integer>()) {
        throw std::invalid_argument("sequence repetition count must be an integer");
    }
    const lua_Integer signedRepetitions = rawRepetitions.as<lua_Integer>();
    const std::size_t repetitions =
        signedRepetitions > 0 ? static_cast<std::size_t>(signedRepetitions) : 0;
    const std::size_t length = nativeSequenceLength(source, kind);
    if (length != 0 &&
        repetitions > std::numeric_limits<std::size_t>::max() / length) {
        throw std::length_error("repeated sequence is too large");
    }
    sol::state_view lua(state);
    sol::object result = createNativeSequence(lua, kind);
    for (std::size_t index = 0; index < repetitions; ++index) {
        appendNativeSequence(lua, result, source, kind);
    }
    return result;
}

sol::object addLists(const sol::object& left, const sol::object& right,
                     sol::this_state state) {
    return concatenateNativeSequences(left, right, ContainerKind::List, state);
}

sol::object multiplyList(const sol::object& left, const sol::object& right,
                         sol::this_state state) {
    return repeatNativeSequence(left, right, ContainerKind::List, state);
}

sol::object addTuples(const sol::object& left, const sol::object& right,
                      sol::this_state state) {
    return concatenateNativeSequences(left, right, ContainerKind::Tuple,
                                      state);
}

sol::object multiplyTuple(const sol::object& left, const sol::object& right,
                          sol::this_state state) {
    return repeatNativeSequence(left, right, ContainerKind::Tuple, state);
}

sol::object copyDict(const sol::object& source, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createDict(lua);
    const NativeDict& sourceDict = source.as<NativeDict&>();
    const sol::table keys = dictKeys(source);
    for (std::size_t index = 0; index < sourceDict.entries.size(); ++index) {
        if (!sourceDict.entries[index].alive) {
            continue;
        }
        setDictEntry(lua, result, keys.raw_get<sol::object>(index + 1),
                     dictEntryValue(lua, source, index), false);
    }
    return result;
}

sol::object constructList(sol::variadic_args arguments, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    bool decodedFromRawTable = false;
    const std::vector<sol::object> values =
        constructorValues(lua, arguments, decodedFromRawTable);
    for (const sol::object& value : values) {
        appendListValue(lua, result, value, decodedFromRawTable, false);
    }
    return result;
}

sol::object constructTuple(sol::variadic_args arguments,
                           sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createTuple(lua);
    bool decodedFromRawTable = false;
    const std::vector<sol::object> values =
        constructorValues(lua, arguments, decodedFromRawTable);
    for (const sol::object& value : values) {
        appendTupleValue(lua, result, value, decodedFromRawTable);
    }
    return result;
}

void collectMappingEntries(
    sol::state_view lua, const sol::object& source,
    std::vector<std::pair<sol::object, sol::object>>& entries,
    bool& decodeJsonNull) {
    decodeJsonNull = false;
    if (source.get_type() == sol::type::table) {
        decodeJsonNull = true;
        const sol::table table = source.as<sol::table>();
        for (const auto& entry : table) {
            sol::object value = entry.second;
            if (isJsonNull(lua, value)) {
                value = nilObject(lua);
            }
            entries.emplace_back(entry.first, value);
        }
        return;
    }
    if (!source.is<NativeDict>()) {
        throw std::invalid_argument(
            "dict mapping source must be a table or dict");
    }
    const NativeDict& dict = source.as<NativeDict&>();
    const sol::table keys = dictKeys(source);
    entries.reserve(dict.length);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        entries.emplace_back(keys.raw_get<sol::object>(index + 1),
                             dictEntryValue(lua, source, index));
    }
}

sol::object constructDict(sol::variadic_args arguments, sol::this_state state) {
    if (arguments.size() > 1) {
        throw std::invalid_argument(
            "dict expects zero arguments or one mapping");
    }
    sol::state_view lua(state);
    sol::object result = createDict(lua);
    if (arguments.size() == 0) {
        return result;
    }
    const sol::object source = arguments.get<sol::object>();
    std::vector<std::pair<sol::object, sol::object>> entries;
    bool decodeJsonNull = false;
    collectMappingEntries(lua, source, entries, decodeJsonNull);
    for (const auto& entry : entries) {
        setDictEntry(lua, result, entry.first, entry.second, decodeJsonNull);
    }
    return result;
}

sol::object dictIndex(sol::this_state state, const sol::object& self,
                      const sol::object& key) {
    sol::state_view lua(state);
    const sol::object member = typeMember(lua, "dict", key);
    if (member.valid() && member.get_type() != sol::type::lua_nil) {
        return member;
    }
    const std::size_t index = findDictEntry(self, key);
    return index == std::numeric_limits<std::size_t>::max()
               ? nilObject(lua)
               : dictEntryValue(lua, self, index);
}

void dictNewIndex(const sol::object& self, const sol::object& key,
                  const sol::object& value, sol::this_state state) {
    setDictEntry(sol::state_view(state), self, key, value, false);
}

sol::object dictGet(const sol::object& self, sol::variadic_args arguments,
                    sol::this_state state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.get expects a key and optional default");
    }
    sol::state_view lua(state);
    const sol::object key = arguments.get<sol::object>();
    const std::size_t index = findDictEntry(self, key);
    if (index != std::numeric_limits<std::size_t>::max()) {
        return dictEntryValue(lua, self, index);
    }
    return arguments.size() == 2 ? arguments.get<sol::object>(1)
                                 : nilObject(lua);
}

sol::object dictSetDefault(const sol::object& self,
                           sol::variadic_args arguments,
                           sol::this_state state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.setdefault expects a key and optional default");
    }
    sol::state_view lua(state);
    const sol::object key = arguments.get<sol::object>();
    const std::size_t index = findDictEntry(self, key);
    if (index != std::numeric_limits<std::size_t>::max()) {
        return dictEntryValue(lua, self, index);
    }
    const sol::object value =
        arguments.size() == 2 ? arguments.get<sol::object>(1) : nilObject(lua);
    setDictEntry(lua, self, key, value, false);
    return value;
}

void dictUpdate(const sol::object& self, const sol::object& source,
                sol::this_state state) {
    sol::state_view lua(state);
    std::vector<std::pair<sol::object, sol::object>> entries;
    bool decodeJsonNull = false;
    collectMappingEntries(lua, source, entries, decodeJsonNull);
    for (const auto& entry : entries) {
        setDictEntry(lua, self, entry.first, entry.second, decodeJsonNull);
    }
}

sol::object dictPop(const sol::object& self, sol::variadic_args arguments,
                    sol::this_state state) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::invalid_argument(
            "dict.pop expects a key and optional default");
    }
    sol::state_view lua(state);
    const sol::object key = arguments.get<sol::object>();
    sol::object result = nilObject(lua);
    if (removeDictEntry(self, key, &result)) {
        return result;
    }
    if (arguments.size() == 2) {
        return arguments.get<sol::object>(1);
    }
    throw std::out_of_range("dict.pop key was not found");
}

bool dictRemove(const sol::object& self, const sol::object& key) {
    return removeDictEntry(self, key, nullptr);
}

void dictClear(const sol::object& self) {
    NativeDict& dict = self.as<NativeDict&>();
    const bool changed = dict.length != 0;
    releaseDictStorage(self);
    if (changed) {
        ++dict.version;
    }
}

bool dictContains(const sol::object& self, const sol::object& key) {
    return findDictEntry(self, key) != std::numeric_limits<std::size_t>::max();
}

sol::object dictKeysList(const sol::object& self, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    const sol::table keys = dictKeys(self);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (dict.entries[index].alive) {
            appendListValue(lua, result, keys.raw_get<sol::object>(index + 1),
                            false, false);
        }
    }
    return result;
}

sol::object dictValuesList(const sol::object& self, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (dict.entries[index].alive) {
            appendListValue(lua, result, dictEntryValue(lua, self, index),
                            false, false);
        }
    }
    return result;
}

sol::object dictItemsList(const sol::object& self, sol::this_state state) {
    sol::state_view lua(state);
    sol::object result = createList(lua);
    const NativeDict& dict = self.as<NativeDict&>();
    const sol::table keys = dictKeys(self);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        sol::object item = createList(lua);
        appendListValue(lua, item, keys.raw_get<sol::object>(index + 1), false,
                        false);
        appendListValue(lua, item, dictEntryValue(lua, self, index), false,
                        false);
        appendListValue(lua, result, item, false, false);
    }
    return result;
}

std::string luaStringValue(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    std::size_t length = 0;
    const char* raw = luaL_tolstring(state, -1, &length);
    std::string result(raw, length);
    lua_pop(state, 2);
    return result;
}

std::string tupleNumberString(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    int integerValid = 0;
    const lua_Integer integer = lua_tointegerx(state, -1, &integerValid);
    lua_pop(state, 1);
    if (integerValid != 0) {
        return std::to_string(static_cast<long long>(integer));
    }
    return luaStringValue(value);
}

std::string quotedString(sol::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (character < 0x20U || character == 0x7fU) {
                    result += "\\x";
                    result.push_back(digits[character >> 4U]);
                    result.push_back(digits[character & 0x0fU]);
                } else {
                    result.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    result.push_back('"');
    return result;
}

std::string tupleItemString(const sol::object& value);

std::string referenceString(const sol::object& value) {
    const char* typeName = "reference";
    switch (value.get_type()) {
        case sol::type::table:
            typeName = "table";
            break;
        case sol::type::function:
            typeName = "function";
            break;
        case sol::type::thread:
            typeName = "thread";
            break;
        case sol::type::userdata:
            typeName = "userdata";
            break;
        case sol::type::lightuserdata:
            typeName = "lightuserdata";
            break;
        default:
            break;
    }
    char address[2 + sizeof(void*) * 2 + 1] = {};
    std::snprintf(address, sizeof(address), "%p", objectIdentity(value));
    return "<" + std::string(typeName) + ":" + address + ">";
}

std::string tupleString(const sol::object& value) {
    const NativeTuple& tuple = value.as<NativeTuple&>();
    const sol::table values = sequenceValues(value);
    std::string result = "(";
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        if (index > 1) {
            result.push_back(',');
        }
        result += tupleItemString(values.raw_get<sol::object>(index));
    }
    if (tuple.length == 1) {
        result.push_back(',');
    }
    result.push_back(')');
    return result;
}

std::string tupleItemString(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::boolean:
            return value.as<bool>() ? "true" : "false";
        case sol::type::number:
            return tupleNumberString(value);
        case sol::type::string:
            return quotedString(value.as<sol::string_view>());
        case sol::type::userdata:
            if (value.is<NativeTuple>()) {
                return tupleString(value);
            }
            [[fallthrough]];
        case sol::type::table:
        case sol::type::function:
        case sol::type::thread:
        case sol::type::lightuserdata:
            return referenceString(value);
        case sol::type::lua_nil:
        case sol::type::none:
            throw std::invalid_argument("tuple elements cannot be nil");
        default:
            return luaStringValue(value);
    }
}

sol::object convertToTable(const sol::object& value,
                           TableConversionContext& context);

sol::object convertedSequence(const sol::object& value,
                              TableConversionContext& context,
                              std::size_t length) {
    const void* identity = objectIdentity(value);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return sol::make_object(context.lua, existing->second);
    }
    sol::table result = context.lua.create_table(static_cast<int>(length), 0);
    const sol::object arrayMetatable =
        context.lua.registry().raw_get<sol::object>(JSON_ARRAY_METATABLE_KEY);
    if (arrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error("cjson array metatable is not registered");
    }
    result[sol::metatable_key] = arrayMetatable.as<sol::table>();
    context.converted.emplace(identity, result);
    const sol::table values = sequenceValues(value);
    for (std::size_t index = 1; index <= length; ++index) {
        result.raw_set(
            index,
            convertToTable(
                exposedValue(context.lua, values.raw_get<sol::object>(index)),
                context));
    }
    return sol::make_object(context.lua, result);
}

sol::object convertedDict(const sol::object& value,
                          TableConversionContext& context) {
    const void* identity = objectIdentity(value);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return sol::make_object(context.lua, existing->second);
    }
    sol::table result = context.lua.create_table();
    context.converted.emplace(identity, result);
    const NativeDict& dict = value.as<NativeDict&>();
    const sol::table keys = dictKeys(value);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        const sol::object sourceKey = keys.raw_get<sol::object>(index + 1);
        const sol::object targetKey =
            sourceKey.is<NativeTuple>()
                ? sol::make_object(context.lua, tupleString(sourceKey))
                : convertToTable(sourceKey, context);
        const sol::object occupied = result.raw_get<sol::object>(targetKey);
        if (occupied.valid() && occupied.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "dict.toTable key conversion would overwrite an existing key");
        }
        result.raw_set(
            targetKey,
            convertToTable(dictEntryValue(context.lua, value, index), context));
    }
    return sol::make_object(context.lua, result);
}

sol::object convertedRawTable(const sol::table& value,
                              TableConversionContext& context) {
    const sol::object source = sol::make_object(context.lua, value);
    const void* identity = objectIdentity(source);
    const auto existing = context.converted.find(identity);
    if (existing != context.converted.end()) {
        return sol::make_object(context.lua, existing->second);
    }
    sol::table result = context.lua.create_table();
    const sol::object arrayMetatable =
        context.lua.registry().raw_get<sol::object>(JSON_ARRAY_METATABLE_KEY);
    if (arrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error("cjson array metatable is not registered");
    }
    const sol::object emptyArrayMetatable =
        context.lua.registry().raw_get<sol::object>(
            JSON_EMPTY_ARRAY_METATABLE_KEY);
    if (emptyArrayMetatable.get_type() != sol::type::table) {
        throw std::runtime_error(
            "cjson empty-array metatable is not registered");
    }
    lua_State* state = context.lua.lua_state();
    const int originalTop = lua_gettop(state);
    value.push();
    const bool hasMetatable = lua_getmetatable(state, -1) != 0;
    bool isJsonArray = false;
    bool isJsonEmptyArray = false;
    if (hasMetatable) {
        arrayMetatable.push();
        isJsonArray = lua_rawequal(state, -1, -2) != 0;
        lua_pop(state, 1);
        emptyArrayMetatable.push();
        isJsonEmptyArray = lua_rawequal(state, -1, -2) != 0;
    }
    lua_settop(state, originalTop);
    if (isJsonArray) {
        result[sol::metatable_key] = arrayMetatable.as<sol::table>();
    } else if (isJsonEmptyArray) {
        result[sol::metatable_key] = emptyArrayMetatable.as<sol::table>();
    }
    context.converted.emplace(identity, result);
    for (const auto& entry : value) {
        result.raw_set(convertToTable(entry.first, context),
                       convertToTable(entry.second, context));
    }
    return sol::make_object(context.lua, result);
}

sol::object convertToTable(const sol::object& value,
                           TableConversionContext& context) {
    if (value.get_type() == sol::type::lua_nil) {
        const sol::object nullValue =
            context.lua.registry().raw_get<sol::object>(JSON_NULL_KEY);
        if (!nullValue.valid() || nullValue.get_type() == sol::type::lua_nil) {
            throw std::runtime_error("cjson.null is not registered");
        }
        return nullValue;
    }
    if (value.is<NativeList>()) {
        return convertedSequence(value, context,
                                 value.as<NativeList&>().length);
    }
    if (value.is<NativeTuple>()) {
        return convertedSequence(value, context,
                                 value.as<NativeTuple&>().length);
    }
    if (value.is<NativeDict>()) {
        return convertedDict(value, context);
    }
    if (value.get_type() == sol::type::table) {
        return convertedRawTable(value.as<sol::table>(), context);
    }
    return value;
}

sol::object containerToTable(const sol::object& value, sol::this_state state) {
    TableConversionContext context{sol::state_view(state)};
    return convertToTable(value, context);
}

sol::object createListDeepCopy(sol::state_view lua, const sol::object&) {
    return createList(lua);
}

void populateListDeepCopy(sol::state_view lua, const sol::object& source,
                          const sol::object& destination,
                          class_runtime::NativeDeepCopyRecurse recurse,
                          void* context) {
    const NativeList& list = source.as<NativeList&>();
    const sol::table values = sequenceValues(source);
    for (std::size_t index = 1; index <= list.length; ++index) {
        appendListValue(
            lua, destination,
            recurse(context,
                    exposedValue(lua, values.raw_get<sol::object>(index))),
            false, false);
    }
}

sol::object buildTupleDeepCopy(sol::state_view lua, const sol::object& source,
                               class_runtime::NativeDeepCopyRecurse recurse,
                               void* context) {
    sol::object destination = createTuple(lua);
    const NativeTuple& tuple = source.as<NativeTuple&>();
    const sol::table values = sequenceValues(source);
    for (std::size_t index = 1; index <= tuple.length; ++index) {
        appendTupleValue(lua, destination,
                         recurse(context, values.raw_get<sol::object>(index)),
                         false);
    }
    return destination;
}

sol::object createDictDeepCopy(sol::state_view lua, const sol::object&) {
    return createDict(lua);
}

void populateDictDeepCopy(sol::state_view lua, const sol::object& source,
                          const sol::object& destination,
                          class_runtime::NativeDeepCopyRecurse recurse,
                          void* context) {
    const NativeDict& dict = source.as<NativeDict&>();
    const sol::table keys = dictKeys(source);
    for (std::size_t index = 0; index < dict.entries.size(); ++index) {
        if (!dict.entries[index].alive) {
            continue;
        }
        setDictEntry(lua, destination,
                     recurse(context, keys.raw_get<sol::object>(index + 1)),
                     recurse(context, dictEntryValue(lua, source, index)),
                     false);
    }
}

void registerList(sol::state_view lua) {
    sol::usertype<NativeList> type =
        lua.new_usertype<NativeList>("list", sol::no_constructor);
    type.set_function("append", &listAppend);
    type.set_function("extend", &listExtend);
    type.set_function("insert", &listInsert);
    type.set_function("pop", &listPop);
    type.set_function("remove", &listRemove);
    type.set_function("clear", &listClear);
    type.set_function("index", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceIndexOf(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("count", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceCount(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("contains", [](const sol::object& self,
                                     sol::variadic_args arguments) {
        return sequenceContains(self, arguments, self.as<NativeList&>().length);
    });
    type.set_function("reverse", &listReverse);
    type.set_function("sort", &listSort);
    type.set_function("copy", &copyList);
    type.set_function("unpack", &sequenceUnpack);
    type.set_function("toTable", &containerToTable);
    type.set_function("__copy", &copyList);
    type[sol::meta_function::index] = [](sol::this_state state,
                                         const sol::object& self,
                                         const sol::object& key) {
        return sequenceIndex(state, self, key, "list",
                             self.as<NativeList&>().length);
    };
    type[sol::meta_function::new_index] = &setListIndex;
    type[sol::meta_function::length] = [](const NativeList& self) {
        return self.length;
    };
    type[sol::meta_function::addition] = &addLists;
    type[sol::meta_function::multiplication] = &multiplyList;
    type[sol::meta_function::equal_to] = [](const sol::object& left,
                                            const sol::object& right) {
        if (!left.is<NativeList>() || !right.is<NativeList>()) {
            return false;
        }
        EqualityContext context;
        return listEqual(left, right, context);
    };
    type[sol::meta_function::pairs] = &sequencePairs;
    sol::table typeTable = lua.globals().get<sol::table>("list");
    sol::table metatable = typeTable[sol::metatable_key];
    metatable.set_function("__call",
                           [](const sol::object&, sol::variadic_args arguments,
                              sol::this_state state) {
                               return constructList(arguments, state);
                           });
    typeTable.raw_set("new", sol::lua_nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createList(lua), &rawListNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyMode::TwoPhase, &createListDeepCopy,
         &populateListDeepCopy, nullptr});
}

void registerTuple(sol::state_view lua) {
    sol::usertype<NativeTuple> type =
        lua.new_usertype<NativeTuple>("tuple", sol::no_constructor);
    type.set_function("index", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceIndexOf(self, arguments, self.as<NativeTuple&>().length);
    });
    type.set_function("count", [](const sol::object& self,
                                  sol::variadic_args arguments) {
        return sequenceCount(self, arguments, self.as<NativeTuple&>().length);
    });
    type.set_function(
        "contains", [](const sol::object& self, sol::variadic_args arguments) {
            return sequenceContains(self, arguments,
                                    self.as<NativeTuple&>().length);
        });
    type.set_function("unpack", &sequenceUnpack);
    type.set_function("toTable", &containerToTable);
    type.set_function("__copy", [](const sol::object& self) {
        return self;
    });
    type[sol::meta_function::index] = [](sol::this_state state,
                                         const sol::object& self,
                                         const sol::object& key) {
        return sequenceIndex(state, self, key, "tuple",
                             self.as<NativeTuple&>().length);
    };
    type[sol::meta_function::new_index] = &rejectTupleWrite;
    type[sol::meta_function::length] = [](const NativeTuple& self) {
        return self.length;
    };
    type[sol::meta_function::addition] = &addTuples;
    type[sol::meta_function::multiplication] = &multiplyTuple;
    type[sol::meta_function::equal_to] = [](const sol::object& left,
                                            const sol::object& right) {
        return left.is<NativeTuple>() && right.is<NativeTuple>() &&
               keyEqual(left, right);
    };
    type[sol::meta_function::to_string] = &tupleString;
    type[sol::meta_function::pairs] = &sequencePairs;
    sol::table typeTable = lua.globals().get<sol::table>("tuple");
    sol::table metatable = typeTable[sol::metatable_key];
    metatable.set_function("__call",
                           [](const sol::object&, sol::variadic_args arguments,
                              sol::this_state state) {
                               return constructTuple(arguments, state);
                           });
    typeTable.raw_set("new", sol::lua_nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createTuple(lua), &rawTupleNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyMode::Deferred, nullptr, nullptr,
         &buildTupleDeepCopy});
}

void registerDict(sol::state_view lua) {
    sol::usertype<NativeDict> type =
        lua.new_usertype<NativeDict>("dict", sol::no_constructor);
    type.set_function("get", &dictGet);
    type.set_function("setdefault", &dictSetDefault);
    type.set_function("update", &dictUpdate);
    type.set_function("pop", &dictPop);
    type.set_function("remove", &dictRemove);
    type.set_function("clear", &dictClear);
    type.set_function("contains", &dictContains);
    type.set_function("keys", &dictKeysList);
    type.set_function("values", &dictValuesList);
    type.set_function("items", &dictItemsList);
    type.set_function("copy", &copyDict);
    type.set_function("toTable", &containerToTable);
    type.set_function("__copy", &copyDict);
    type[sol::meta_function::index] = &dictIndex;
    type[sol::meta_function::new_index] = &dictNewIndex;
    type[sol::meta_function::length] = [](const NativeDict& self) {
        return self.length;
    };
    type[sol::meta_function::equal_to] = [](const sol::object& left,
                                            const sol::object& right) {
        if (!left.is<NativeDict>() || !right.is<NativeDict>()) {
            return false;
        }
        EqualityContext context;
        return dictEqual(left, right, context);
    };
    type[sol::meta_function::pairs] = &nativeDictPairs;
    sol::table typeTable = lua.globals().get<sol::table>("dict");
    sol::table metatable = typeTable[sol::metatable_key];
    metatable.set_function("__call",
                           [](const sol::object&, sol::variadic_args arguments,
                              sol::this_state state) {
                               return constructDict(arguments, state);
                           });
    typeTable.raw_set("new", sol::lua_nil);
    maskNewConstructor(typeTable);
    overrideNewIndex(createDict(lua), &rawDictNewIndex);
    class_runtime::registerNativeDeepCopyProtocol(
        lua, typeTable,
        {class_runtime::NativeDeepCopyMode::TwoPhase, &createDictDeepCopy,
         &populateDictDeepCopy, nullptr});
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

}  // namespace

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
