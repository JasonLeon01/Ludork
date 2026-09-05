#pragma once

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::container_runtime::detail {

inline constexpr const char* ORIGINAL_IPAIRS_KEY =
    "LudorkStandard.ContainerOriginalIpairs";
inline constexpr const char* LESS_THAN_KEY = "LudorkStandard.ContainerLessThan";
inline constexpr const char* JSON_NULL_KEY = "LuaSF.JsonNullSentinel";
inline constexpr const char* JSON_ARRAY_METATABLE_KEY =
    "LuaSF.JsonArrayMetatable";
inline constexpr const char* JSON_EMPTY_ARRAY_METATABLE_KEY =
    "LuaSF.JsonEmptyArrayMetatable";
inline constexpr std::uint64_t HASH_OFFSET = 1469598103934665603ULL;
inline constexpr std::uint64_t HASH_PRIME = 1099511628211ULL;

extern unsigned char nilSentinelStorage;

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

sol::object nilObject(sol::state_view lua);
const void* objectIdentity(const sol::object& value);
bool rawEqual(const sol::object& left, const sol::object& right);
bool luaEqual(const sol::object& left, const sol::object& right);
ContainerKind containerKind(const sol::object& value);
bool isJsonNull(sol::state_view lua, const sol::object& value);
sol::object storedValue(sol::state_view lua, const sol::object& value,
                        bool decodeJsonNull);
sol::object exposedValue(sol::state_view lua, const sol::object& value);
sol::table uservalueRoot(const sol::object& value);
sol::table sequenceValues(const sol::object& value);
sol::table dictKeys(const sol::object& value);
sol::table dictValues(const sol::object& value);
sol::object createList(sol::state_view lua);
sol::object createTuple(sol::state_view lua);
sol::object createDict(sol::state_view lua);
std::size_t sequenceLength(const sol::object& source);
bool isSequenceSource(const sol::object& source);
sol::object sequenceItem(sol::state_view lua, const sol::object& source,
                         std::size_t index, bool exposeNil);
void appendListValue(sol::state_view lua, const sol::object& target,
                     const sol::object& value, bool decodeJsonNull,
                     bool structural = true);
void appendTupleValue(sol::state_view lua, const sol::object& target,
                      const sol::object& value, bool decodeJsonNull);
std::vector<sol::object> constructorValues(sol::state_view lua,
                                           sol::variadic_args arguments,
                                           bool& decodedFromRawTable);
std::size_t findDictEntry(const sol::object& target, const sol::object& key);
void setDictEntry(sol::state_view lua, const sol::object& target,
                  const sol::object& key, const sol::object& value,
                  bool decodeJsonNull);
sol::object dictEntryValue(sol::state_view lua, const sol::object& target,
                           std::size_t index);
void releaseDictStorage(const sol::object& target);
bool removeDictEntry(const sol::object& target, const sol::object& key,
                     sol::object* removedValue);
bool keyEqual(const sol::object& left, const sol::object& right);
bool listEqual(const sol::object& left, const sol::object& right,
               EqualityContext& context);
bool dictEqual(const sol::object& left, const sol::object& right,
               EqualityContext& context);
bool valueEqual(const sol::object& left, const sol::object& right);
lua_Integer checkedIndex(const sol::object& value, const char* name);
sol::object typeMember(sol::state_view lua, const char* typeName,
                       const sol::object& key);
sol::object sequenceIndex(sol::this_state state, const sol::object& self,
                          const sol::object& key, const char* typeName,
                          std::size_t length);
bool callComparator(const sol::protected_function& comparator,
                    const sol::object& left, const sol::object& right);
int lessThan(lua_State* state);
std::tuple<sol::function, sol::object, sol::object> sequencePairs(
    const sol::object& self, sol::this_state state);
std::tuple<sol::function, sol::object, sol::object> nativeDictPairs(
    const sol::object& self, sol::this_state state);
void maskNewConstructor(sol::table typeTable);
void overrideNewIndex(const sol::object& sample, lua_CFunction newIndex);
std::string tupleString(const sol::object& value);
sol::object containerToTable(const sol::object& value, sol::this_state state);
void registerIpairs(sol::state_view lua);

void registerList(sol::state_view lua);
void registerTuple(sol::state_view lua);
void registerDict(sol::state_view lua);

}  // namespace ludork::standard::container_runtime::detail
