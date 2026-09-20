#pragma once

#include <LuaGlue/LuaGlue.hpp>

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

struct EqualityContext {
    struct ObjectPairHash {
        std::size_t operator()(const ObjectPair& value) const noexcept;
    };

    std::unordered_set<ObjectPair, ObjectPairHash> visited;
};

lua_glue::Object nilObject(lua_glue::StateView lua);
const void* objectIdentity(const lua_glue::Object& value);
bool rawEqual(const lua_glue::Object& left, const lua_glue::Object& right);
bool luaEqual(const lua_glue::Object& left, const lua_glue::Object& right);
ContainerKind containerKind(const lua_glue::Object& value);
bool isJsonNull(lua_glue::StateView lua, const lua_glue::Object& value);
lua_glue::Object storedValue(lua_glue::StateView lua,
                             const lua_glue::Object& value,
                             bool decodeJsonNull);
lua_glue::Object exposedValue(lua_glue::StateView lua,
                              const lua_glue::Object& value);
lua_glue::Table uservalueRoot(const lua_glue::Object& value);
lua_glue::Table sequenceValues(const lua_glue::Object& value);
lua_glue::Table dictKeys(const lua_glue::Object& value);
lua_glue::Table dictValues(const lua_glue::Object& value);
lua_glue::Object createList(lua_glue::StateView lua);
lua_glue::Object createTuple(lua_glue::StateView lua);
lua_glue::Object createDict(lua_glue::StateView lua);
std::size_t sequenceLength(const lua_glue::Object& source);
bool isSequenceSource(const lua_glue::Object& source);
lua_glue::Object sequenceItem(lua_glue::StateView lua,
                              const lua_glue::Object& source, std::size_t index,
                              bool exposeNil);
void appendListValue(lua_glue::StateView lua, const lua_glue::Object& target,
                     const lua_glue::Object& value, bool decodeJsonNull,
                     bool structural = true);
void appendTupleValue(lua_glue::StateView lua, const lua_glue::Object& target,
                      const lua_glue::Object& value, bool decodeJsonNull);
std::vector<lua_glue::Object> constructorValues(lua_glue::StateView lua,
                                                lua_glue::Arguments arguments,
                                                bool& decodedFromRawTable);
std::size_t findDictEntry(const lua_glue::Object& target,
                          const lua_glue::Object& key);
void setDictEntry(lua_glue::StateView lua, const lua_glue::Object& target,
                  const lua_glue::Object& key, const lua_glue::Object& value,
                  bool decodeJsonNull);
lua_glue::Object dictEntryValue(lua_glue::StateView lua,
                                const lua_glue::Object& target,
                                std::size_t index);
void releaseDictStorage(const lua_glue::Object& target);
bool removeDictEntry(const lua_glue::Object& target,
                     const lua_glue::Object& key,
                     lua_glue::Object* removedValue);
bool keyEqual(const lua_glue::Object& left, const lua_glue::Object& right);
bool listEqual(const lua_glue::Object& left, const lua_glue::Object& right,
               EqualityContext& context);
bool dictEqual(const lua_glue::Object& left, const lua_glue::Object& right,
               EqualityContext& context);
bool valueEqual(const lua_glue::Object& left, const lua_glue::Object& right);
lua_Integer checkedIndex(const lua_glue::Object& value, const char* name);
lua_glue::Object typeMember(lua_glue::StateView lua, const char* typeName,
                            const lua_glue::Object& key);
lua_glue::Object sequenceIndex(lua_glue::ThisState state,
                               const lua_glue::Object& self,
                               const lua_glue::Object& key,
                               const char* typeName, std::size_t length);
bool callComparator(const lua_glue::Function& comparator,
                    const lua_glue::Object& left,
                    const lua_glue::Object& right);
int lessThan(lua_State* state);
lua_glue::MultipleResults sequencePairs(const lua_glue::Object& self,
                                        lua_glue::ThisState state);
lua_glue::MultipleResults nativeDictPairs(const lua_glue::Object& self,
                                          lua_glue::ThisState state);
void maskNewConstructor(lua_glue::Table typeTable);
void overrideNewIndex(const lua_glue::Object& sample, lua_CFunction newIndex);
std::string tupleString(const lua_glue::Object& value);
lua_glue::Object containerToTable(const lua_glue::Object& value,
                                  lua_glue::ThisState state);
void registerIpairs(lua_glue::StateView lua);

void registerList(lua_glue::StateView lua);
void registerTuple(lua_glue::StateView lua);
void registerDict(lua_glue::StateView lua);

}  // namespace ludork::standard::container_runtime::detail
