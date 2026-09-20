#include "Detail/Hierarchy.hpp"
#include "RuntimeState.hpp"

#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/TypedFields.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ludork::standard::class_runtime::detail {

namespace {

void pushRawField(lua_State* state, int tableIndex, const char* name) {
    tableIndex = lua_absindex(state, tableIndex);
    lua_pushstring(state, name);
    lua_rawget(state, tableIndex);
}

bool isScriptClassAt(lua_State* state, int index) {
    pushRawField(state, index, protocol::CLASS_MARKER_FIELD);
    const bool result = lua_isboolean(state, -1) && lua_toboolean(state, -1);
    lua_pop(state, 1);
    return result;
}

void pushLookupOwners(lua_State* state, int classIndex, const char* category) {
    classIndex = lua_absindex(state, classIndex);
    luaL_checktype(state, classIndex, LUA_TTABLE);
    pushRawField(state, classIndex, LOOKUP_CACHE_FIELD);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushstring(state, LOOKUP_CACHE_FIELD);
        lua_pushvalue(state, -2);
        lua_rawset(state, classIndex);
    }
    const int cacheIndex = lua_absindex(state, -1);
    pushRawField(state, cacheIndex, category);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushstring(state, category);
        lua_pushvalue(state, -2);
        lua_rawset(state, cacheIndex);
    }
    lua_remove(state, cacheIndex);
}

int lookupOwnersThunk(lua_State* state) {
    luaL_checkstack(state, 8, "Class lookup stack cannot grow");
    pushLookupOwners(state, 1,
                     static_cast<const char*>(lua_touserdata(state, 2)));
    return 1;
}

int returnCachedLookup(lua_State* state, int ownersIndex, int valueIndex,
                       bool resolved) {
    valueIndex = valueIndex == 0 ? 0 : lua_absindex(state, valueIndex);
    lua_pushvalue(state, ownersIndex);
    if (valueIndex == 0) {
        lua_pushnil(state);
    } else {
        lua_pushvalue(state, valueIndex);
    }
    lua_pushboolean(state, resolved);
    return 3;
}

int cachedLookupThunk(lua_State* state) {
    luaL_checkstack(state, 12, "Class lookup stack cannot grow");
    const char* category = static_cast<const char*>(lua_touserdata(state, 3));
    const bool accessor = lua_toboolean(state, 4) != 0;
    pushLookupOwners(state, 1, category);
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushvalue(state, 2);
    lua_rawget(state, ownersIndex);
    if (accessor && lua_isboolean(state, -1) && !lua_toboolean(state, -1)) {
        return returnCachedLookup(state, ownersIndex, 0, true);
    }
    if (lua_istable(state, -1)) {
        const int ownerIndex = lua_absindex(state, -1);
        // Native descriptors have the same metadata field names, but their
        // receivers must follow native-root initialization and shadow replay.
        if (accessor && isScriptClassAt(state, ownerIndex)) {
            pushRawField(state, ownerIndex, category);
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (!lua_isnil(state, -1)) {
                    return returnCachedLookup(state, ownersIndex, -1, true);
                }
            }
        } else if (!accessor) {
            lua_pushvalue(state, 2);
            lua_rawget(state, ownerIndex);
            if (!lua_isnil(state, -1) ||
                hasExplicitNilField(state, ownerIndex, 2)) {
                return returnCachedLookup(state, ownersIndex, -1, true);
            }
        }
        lua_pushvalue(state, 2);
        lua_pushnil(state);
        lua_rawset(state, ownersIndex);
    }
    return returnCachedLookup(state, ownersIndex, 0, false);
}

int scanLookupThunk(lua_State* state) {
    luaL_checkstack(state, 12, "Class lookup stack cannot grow");
    luaL_checktype(state, 1, LUA_TTABLE);
    luaL_checktype(state, 2, LUA_TTABLE);
    const char* category = static_cast<const char*>(lua_touserdata(state, 4));
    const bool accessor = lua_toboolean(state, 5) != 0;
    for (std::size_t index = 1; index <= lua_rawlen(state, 2); ++index) {
        lua_geti(state, 2, static_cast<lua_Integer>(index));
        if (lua_istable(state, -1) && isScriptClassAt(state, -1)) {
            const int ownerIndex = lua_absindex(state, -1);
            bool present = false;
            if (accessor) {
                pushRawField(state, ownerIndex, category);
                if (lua_istable(state, -1)) {
                    lua_pushvalue(state, 3);
                    lua_rawget(state, -2);
                    present = !lua_isnil(state, -1);
                }
            } else {
                lua_pushvalue(state, 3);
                lua_rawget(state, ownerIndex);
                present = !lua_isnil(state, -1) ||
                          hasExplicitNilField(state, ownerIndex, 3);
            }
            if (present) {
                lua_pushvalue(state, 3);
                lua_pushvalue(state, ownerIndex);
                lua_rawset(state, 1);
                lua_pushboolean(state, true);
                return 2;
            }
        }
        lua_settop(state, 5);
    }
    if (accessor) {
        lua_pushvalue(state, 3);
        lua_pushboolean(state, false);
        lua_rawset(state, 1);
    }
    lua_pushnil(state);
    lua_pushboolean(state, false);
    return 2;
}

lua_glue::Object findLookup(lua_glue::StateView lua,
                            const lua_glue::Table& classTable,
                            const lua_glue::Object& key, const char* category,
                            bool accessor, bool* found) {
    if (found != nullptr) {
        *found = false;
    }
    lua_State* state = lua.lua_state();
    lua_glue::detail::AccessScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error("Class lookup requires an active Lua state");
    }
    lua_glue::StackGuard stack(state);
    if (!lua_checkstack(state, 10)) {
        throw std::runtime_error("Class lookup stack cannot grow");
    }
    classTable.push(state);
    const int classIndex = lua_absindex(state, -1);
    key.push(state);
    const int keyIndex = lua_absindex(state, -1);
    lua_pushcfunction(state, cachedLookupThunk);
    lua_pushvalue(state, classIndex);
    lua_pushvalue(state, keyIndex);
    lua_pushlightuserdata(state, const_cast<char*>(category));
    lua_pushboolean(state, accessor);
    lua_glue::ProtectedStackCall(state, 4, 3);
    if (lua_toboolean(state, -1)) {
        lua_glue::Object result(state, -2);
        if (found != nullptr) {
            *found = true;
        }
        return result;
    }
    lua_pop(state, 2);
    const int ownersIndex = lua_absindex(state, -1);
    const lua_glue::Table mro = getMro(lua, classTable);
    lua_pushcfunction(state, scanLookupThunk);
    lua_pushvalue(state, ownersIndex);
    mro.push(state);
    lua_pushvalue(state, keyIndex);
    lua_pushlightuserdata(state, const_cast<char*>(category));
    lua_pushboolean(state, accessor);
    lua_glue::ProtectedStackCall(state, 5, 2);
    lua_glue::Object result(state, -2);
    if (found != nullptr) {
        *found = lua_toboolean(state, -1) != 0;
    }
    return result;
}

}  // namespace

lua_glue::Table classLookupOwners(lua_glue::StateView lua,
                                  lua_glue::Table classTable,
                                  const char* category) {
    lua_State* state = lua.lua_state();
    lua_glue::detail::AccessScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error("Class lookup requires an active Lua state");
    }
    lua_glue::StackGuard stack(state);
    if (!lua_checkstack(state, 4)) {
        throw std::runtime_error("Class lookup stack cannot grow");
    }
    lua_pushcfunction(state, lookupOwnersThunk);
    classTable.push(state);
    lua_pushlightuserdata(state, const_cast<char*>(category));
    lua_glue::ProtectedStackCall(state, 2, 1);
    return lua_glue::Table(state, -1);
}

void invalidateClassLookup(lua_glue::StateView lua,
                           lua_glue::Table classTable) {
    const lua_glue::Object rawVersion =
        classTable.raw_get<lua_glue::Object>(LOOKUP_VERSION_FIELD);
    const lua_Integer version =
        rawVersion.is<lua_Integer>() ? rawVersion.as<lua_Integer>() : 0;
    classTable.raw_set(LOOKUP_VERSION_FIELD, version + 1);
    classTable.raw_set(LOOKUP_CACHE_FIELD, lua_glue::nil);
    const lua_glue::Object rawSubclasses =
        classTable.raw_get<lua_glue::Object>(SUBCLASSES_FIELD);
    if (!rawSubclasses.is<lua_glue::Table>()) {
        return;
    }
    for (const auto& entry : rawSubclasses.as<lua_glue::Table>()) {
        if (entry.first.is<lua_glue::Table>()) {
            invalidateClassLookup(lua, entry.first.as<lua_glue::Table>());
        }
    }
}

void registerSubclass(lua_glue::StateView lua, lua_glue::Table base,
                      const lua_glue::Table& subclass) {
    const lua_glue::Object rawSubclasses =
        base.raw_get<lua_glue::Object>(SUBCLASSES_FIELD);
    lua_glue::Table subclasses = rawSubclasses.is<lua_glue::Table>()
                                     ? rawSubclasses.as<lua_glue::Table>()
                                     : createWeakTable(lua, "k");
    if (!rawSubclasses.is<lua_glue::Table>()) {
        base.raw_set(SUBCLASSES_FIELD, subclasses);
    }
    subclasses.raw_set(subclass, true);
}

std::vector<lua_glue::Table> tableList(const lua_glue::Table& values) {
    std::vector<lua_glue::Table> result;
    result.reserve(values.size());
    for (std::size_t index = 1; index <= values.size(); ++index) {
        const lua_glue::Object value = values[index];
        if (value.is<lua_glue::Table>()) {
            result.push_back(value.as<lua_glue::Table>());
        }
    }
    return result;
}

bool containsTable(const std::vector<lua_glue::Table>& values,
                   std::size_t start, const lua_glue::Table& target) {
    for (std::size_t index = start; index < values.size(); ++index) {
        if (objectsRawEqual(values[index], target)) {
            return true;
        }
    }
    return false;
}

void ensureMroSet(lua_glue::StateView lua, lua_glue::Table type,
                  const lua_glue::Table& mro, const char* setName) {
    if (type.raw_get<lua_glue::Object>(setName).is<lua_glue::Table>()) {
        return;
    }
    lua_glue::Table mroSet = lua.create_table();
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object entry = mro.raw_get<lua_glue::Object>(index);
        if (entry.is<lua_glue::Table>()) {
            mroSet.raw_set(entry, true);
        }
    }
    type.raw_set(setName, mroSet);
}

std::vector<lua_glue::Table> createMro(const lua_glue::Table& type,
                                       const lua_glue::Table& bases,
                                       MroKind kind) {
    lua_glue::StateView lua(type.lua_state());
    std::vector<std::vector<lua_glue::Table>> sequences;
    for (const lua_glue::Table& base : tableList(bases)) {
        const lua_glue::Table mro = kind == MroKind::Runtime
                                        ? getMro(lua, base)
                                        : getNativeMro(lua, base);
        sequences.push_back(tableList(mro));
    }
    sequences.push_back(tableList(bases));
    std::vector<std::size_t> positions(sequences.size(), 0);
    std::vector<lua_glue::Table> result{type};
    while (true) {
        bool hasRemaining = false;
        bool selected = false;
        for (std::size_t sequenceIndex = 0; sequenceIndex < sequences.size();
             ++sequenceIndex) {
            const std::vector<lua_glue::Table>& sequence =
                sequences[sequenceIndex];
            if (positions[sequenceIndex] >= sequence.size()) {
                continue;
            }
            hasRemaining = true;
            const lua_glue::Table& candidate =
                sequence[positions[sequenceIndex]];
            bool appearsInTail = false;
            for (std::size_t otherIndex = 0; otherIndex < sequences.size();
                 ++otherIndex) {
                if (containsTable(sequences[otherIndex],
                                  positions[otherIndex] + 1, candidate)) {
                    appearsInTail = true;
                    break;
                }
            }
            if (appearsInTail) {
                continue;
            }
            result.push_back(candidate);
            for (std::size_t otherIndex = 0; otherIndex < sequences.size();
                 ++otherIndex) {
                const std::vector<lua_glue::Table>& other =
                    sequences[otherIndex];
                if (positions[otherIndex] < other.size() &&
                    objectsRawEqual(other[positions[otherIndex]], candidate)) {
                    ++positions[otherIndex];
                }
            }
            selected = true;
            break;
        }
        if (!hasRemaining) {
            return result;
        }
        if (!selected) {
            throw std::invalid_argument("Inconsistent class inheritance order");
        }
    }
}

lua_glue::Table getMro(lua_glue::StateView lua, lua_glue::Table type) {
    const lua_glue::Object rawMro = type.raw_get<lua_glue::Object>(MRO_FIELD);
    if (rawMro.is<lua_glue::Table>()) {
        const lua_glue::Table mro = rawMro.as<lua_glue::Table>();
        ensureMroSet(lua, type, mro, MRO_SET_FIELD);
        return mro;
    }
    const lua_glue::Object rawRuntimeMro =
        type.raw_get<lua_glue::Object>(RUNTIME_MRO_FIELD);
    if (rawRuntimeMro.is<lua_glue::Table>()) {
        const lua_glue::Table mro = rawRuntimeMro.as<lua_glue::Table>();
        ensureMroSet(lua, type, mro, RUNTIME_MRO_SET_FIELD);
        return mro;
    }
    lua_glue::Table bases = lua.create_table();
    lua_glue::Object rawBases =
        type.raw_get<lua_glue::Object>(RUNTIME_BASES_FIELD);
    if (!rawBases.is<lua_glue::Table>()) {
        rawBases = type.raw_get<lua_glue::Object>(NATIVE_BASES_FIELD);
    }
    if (rawBases.is<lua_glue::Table>()) {
        bases = rawBases.as<lua_glue::Table>();
    }
    lua_glue::Table result = lua.create_table();
    if (bases.size() == 0) {
        result.add(type);
    } else {
        for (const lua_glue::Table& entry :
             createMro(type, bases, MroKind::Runtime)) {
            result.add(entry);
        }
    }
    type.raw_set(RUNTIME_MRO_FIELD, result);
    ensureMroSet(lua, type, result, RUNTIME_MRO_SET_FIELD);
    return result;
}

lua_glue::Table getNativeMro(lua_glue::StateView lua, lua_glue::Table type) {
    const lua_glue::Object rawMro =
        type.raw_get<lua_glue::Object>(NATIVE_MRO_FIELD);
    if (rawMro.is<lua_glue::Table>()) {
        const lua_glue::Table mro = rawMro.as<lua_glue::Table>();
        ensureMroSet(lua, type, mro, NATIVE_MRO_SET_FIELD);
        return mro;
    }
    lua_glue::Table bases = lua.create_table();
    const lua_glue::Object rawBases =
        type.raw_get<lua_glue::Object>(NATIVE_BASES_FIELD);
    if (rawBases.is<lua_glue::Table>()) {
        bases = rawBases.as<lua_glue::Table>();
    }
    lua_glue::Table result = lua.create_table();
    if (bases.size() == 0) {
        result.add(type);
    } else {
        for (const lua_glue::Table& entry :
             createMro(type, bases, MroKind::Native)) {
            result.add(entry);
        }
    }
    type.raw_set(NATIVE_MRO_FIELD, result);
    ensureMroSet(lua, type, result, NATIVE_MRO_SET_FIELD);
    return result;
}

lua_glue::Table getBases(lua_glue::StateView lua,
                         const lua_glue::Table& classTable) {
    const lua_glue::Object value =
        classTable.raw_get<lua_glue::Object>(BASES_FIELD);
    return value.is<lua_glue::Table>() ? value.as<lua_glue::Table>()
                                       : lua.create_table();
}

lua_glue::Object findAccessor(lua_glue::StateView lua,
                              const lua_glue::Table& classTable,
                              const char* collectionName,
                              const lua_glue::Object& key) {
    return findLookup(lua, classTable, key, collectionName, true, nullptr);
}

lua_glue::Object findScriptMember(lua_glue::StateView lua,
                                  const lua_glue::Table& classTable,
                                  const lua_glue::Object& key, bool* found) {
    return findLookup(lua, classTable, key, "scriptMembers", false, found);
}

lua_glue::Object findClassOverride(lua_glue::StateView lua,
                                   const lua_glue::Table& classTable,
                                   const lua_glue::Object& key) {
    const lua_glue::Table mro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object value =
            rawType.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
        if ((value.valid() && value.get_type() != lua_glue::Type::Nil) ||
            hasExplicitNilField(lua, rawType, key)) {
            return value;
        }
    }
    return nilObject(lua);
}

bool derivesFrom(lua_glue::StateView lua, const lua_glue::Table& classTable,
                 const lua_glue::Table& targetClass) {
    lua_glue::Object rawSet =
        classTable.raw_get<lua_glue::Object>(MRO_SET_FIELD);
    if (!rawSet.is<lua_glue::Table>()) {
        rawSet = classTable.raw_get<lua_glue::Object>(RUNTIME_MRO_SET_FIELD);
    }
    if (!rawSet.is<lua_glue::Table>()) {
        getMro(lua, classTable);
        rawSet = classTable.raw_get<lua_glue::Object>(RUNTIME_MRO_SET_FIELD);
    }
    if (!rawSet.is<lua_glue::Table>()) {
        return false;
    }
    const lua_glue::Object result =
        rawSet.as<lua_glue::Table>().raw_get<lua_glue::Object>(targetClass);
    return result.is<bool>() && result.as<bool>();
}

lua_glue::Table resolverMro(lua_glue::StateView lua,
                            const lua_glue::Table& classTable) {
    return getMro(lua, classTable);
}

}  // namespace ludork::standard::class_runtime::detail
