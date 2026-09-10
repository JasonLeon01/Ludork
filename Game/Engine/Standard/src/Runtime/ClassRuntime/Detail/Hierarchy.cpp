#include "Detail/Hierarchy.hpp"

#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/TypedFields.hpp"

#include <sol2/sol.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ludork::standard::class_runtime::detail {

sol::table classLookupOwners(sol::state_view lua, sol::table classTable,
                             const char* category) {
    sol::object rawCache = classTable.raw_get<sol::object>("__lookupCache");
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        classTable.raw_set("__lookupCache", cache);
    }
    const sol::object rawOwners = cache.raw_get<sol::object>(category);
    if (rawOwners.is<sol::table>()) {
        return rawOwners.as<sol::table>();
    }
    sol::table owners = lua.create_table();
    cache.raw_set(category, owners);
    return owners;
}

void invalidateClassLookup(sol::state_view lua, sol::table classTable) {
    const sol::object rawVersion =
        classTable.raw_get<sol::object>("__lookupVersion");
    const lua_Integer version =
        rawVersion.is<lua_Integer>() ? rawVersion.as<lua_Integer>() : 0;
    classTable.raw_set("__lookupVersion", version + 1);
    classTable.raw_set("__lookupCache", sol::lua_nil);
    const sol::object rawSubclasses =
        classTable.raw_get<sol::object>("__subclasses");
    if (!rawSubclasses.is<sol::table>()) {
        return;
    }
    for (const auto& entry : rawSubclasses.as<sol::table>()) {
        if (entry.first.is<sol::table>()) {
            invalidateClassLookup(lua, entry.first.as<sol::table>());
        }
    }
}

void registerSubclass(sol::state_view lua, sol::table base,
                      const sol::table& subclass) {
    const sol::object rawSubclasses = base.raw_get<sol::object>("__subclasses");
    sol::table subclasses = rawSubclasses.is<sol::table>()
                                ? rawSubclasses.as<sol::table>()
                                : createWeakTable(lua, "k");
    if (!rawSubclasses.is<sol::table>()) {
        base.raw_set("__subclasses", subclasses);
    }
    subclasses.raw_set(subclass, true);
}

std::vector<sol::table> tableList(const sol::table& values) {
    std::vector<sol::table> result;
    result.reserve(values.size());
    for (std::size_t index = 1; index <= values.size(); ++index) {
        const sol::object value = values[index];
        if (value.is<sol::table>()) {
            result.push_back(value.as<sol::table>());
        }
    }
    return result;
}

bool containsTable(const std::vector<sol::table>& values, std::size_t start,
                   const sol::table& target) {
    for (std::size_t index = start; index < values.size(); ++index) {
        if (objectsRawEqual(values[index], target)) {
            return true;
        }
    }
    return false;
}

void ensureMroSet(sol::state_view lua, sol::table type, const sol::table& mro,
                  const char* setName) {
    if (type.raw_get<sol::object>(setName).is<sol::table>()) {
        return;
    }
    sol::table mroSet = lua.create_table();
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object entry = mro.raw_get<sol::object>(index);
        if (entry.is<sol::table>()) {
            mroSet.raw_set(entry, true);
        }
    }
    type.raw_set(setName, mroSet);
}

std::vector<sol::table> createMro(const sol::table& type,
                                  const sol::table& bases, MroKind kind) {
    sol::state_view lua(type.lua_state());
    std::vector<std::vector<sol::table>> sequences;
    for (const sol::table& base : tableList(bases)) {
        const sol::table mro = kind == MroKind::Runtime
                                   ? getMro(lua, base)
                                   : getNativeMro(lua, base);
        sequences.push_back(tableList(mro));
    }
    sequences.push_back(tableList(bases));
    std::vector<std::size_t> positions(sequences.size(), 0);
    std::vector<sol::table> result{type};
    while (true) {
        bool hasRemaining = false;
        bool selected = false;
        for (std::size_t sequenceIndex = 0; sequenceIndex < sequences.size();
             ++sequenceIndex) {
            const std::vector<sol::table>& sequence = sequences[sequenceIndex];
            if (positions[sequenceIndex] >= sequence.size()) {
                continue;
            }
            hasRemaining = true;
            const sol::table& candidate = sequence[positions[sequenceIndex]];
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
                const std::vector<sol::table>& other = sequences[otherIndex];
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

sol::table getMro(sol::state_view lua, sol::table type) {
    const sol::object rawMro = type.raw_get<sol::object>("__mro");
    if (rawMro.is<sol::table>()) {
        const sol::table mro = rawMro.as<sol::table>();
        ensureMroSet(lua, type, mro, "__mroSet");
        return mro;
    }
    const sol::object rawRuntimeMro = type.raw_get<sol::object>("__runtimeMro");
    if (rawRuntimeMro.is<sol::table>()) {
        const sol::table mro = rawRuntimeMro.as<sol::table>();
        ensureMroSet(lua, type, mro, "__runtimeMroSet");
        return mro;
    }
    sol::table bases = lua.create_table();
    sol::object rawBases = type.raw_get<sol::object>("__runtimeBases");
    if (!rawBases.is<sol::table>()) {
        rawBases = type.raw_get<sol::object>("__nativeBases");
    }
    if (rawBases.is<sol::table>()) {
        bases = rawBases.as<sol::table>();
    }
    sol::table result = lua.create_table();
    if (bases.size() == 0) {
        result.add(type);
    } else {
        for (const sol::table& entry :
             createMro(type, bases, MroKind::Runtime)) {
            result.add(entry);
        }
    }
    type.raw_set("__runtimeMro", result);
    ensureMroSet(lua, type, result, "__runtimeMroSet");
    return result;
}

sol::table getNativeMro(sol::state_view lua, sol::table type) {
    const sol::object rawMro = type.raw_get<sol::object>("__nativeMro");
    if (rawMro.is<sol::table>()) {
        const sol::table mro = rawMro.as<sol::table>();
        ensureMroSet(lua, type, mro, "__nativeMroSet");
        return mro;
    }
    sol::table bases = lua.create_table();
    const sol::object rawBases = type.raw_get<sol::object>("__nativeBases");
    if (rawBases.is<sol::table>()) {
        bases = rawBases.as<sol::table>();
    }
    sol::table result = lua.create_table();
    if (bases.size() == 0) {
        result.add(type);
    } else {
        for (const sol::table& entry :
             createMro(type, bases, MroKind::Native)) {
            result.add(entry);
        }
    }
    type.raw_set("__nativeMro", result);
    ensureMroSet(lua, type, result, "__nativeMroSet");
    return result;
}

sol::table getBases(sol::state_view lua, const sol::table& classTable) {
    const sol::object value = classTable.raw_get<sol::object>("__bases");
    return value.is<sol::table>() ? value.as<sol::table>() : lua.create_table();
}

sol::object findAccessor(sol::state_view lua, const sol::table& classTable,
                         const char* collectionName, const sol::object& key) {
    sol::table owners = classLookupOwners(lua, classTable, collectionName);
    const sol::object rawOwner = owners.raw_get<sol::object>(key);
    if (rawOwner.is<bool>() && !rawOwner.as<bool>()) {
        return nilObject(lua);
    }
    if (rawOwner.is<sol::table>()) {
        const sol::object rawCollection =
            rawOwner.as<sol::table>().raw_get<sol::object>(collectionName);
        if (rawCollection.is<sol::table>()) {
            const sol::object accessor =
                rawCollection.as<sol::table>().raw_get<sol::object>(key);
            if (accessor.valid() && accessor.get_type() != sol::type::lua_nil) {
                return accessor;
            }
        }
        owners.raw_set(key, sol::lua_nil);
    }
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object rawCollection =
            rawType.as<sol::table>().raw_get<sol::object>(collectionName);
        if (!rawCollection.is<sol::table>()) {
            continue;
        }
        const sol::object accessor =
            rawCollection.as<sol::table>().raw_get<sol::object>(key);
        if (accessor.valid() && accessor.get_type() != sol::type::lua_nil) {
            owners.raw_set(key, rawType);
            return accessor;
        }
    }
    owners.raw_set(key, false);
    return nilObject(lua);
}

sol::object findScriptMember(sol::state_view lua, const sol::table& classTable,
                             const sol::object& key, bool* found) {
    if (found != nullptr) {
        *found = false;
    }
    sol::table owners = classLookupOwners(lua, classTable, "scriptMembers");
    const sol::object rawOwner = owners.raw_get<sol::object>(key);
    if (rawOwner.is<sol::table>()) {
        const sol::object cached =
            rawOwner.as<sol::table>().raw_get<sol::object>(key);
        if ((cached.valid() && cached.get_type() != sol::type::lua_nil) ||
            hasExplicitNilField(lua, rawOwner, key)) {
            if (found != nullptr) {
                *found = true;
            }
            return cached;
        }
        owners.raw_set(key, sol::lua_nil);
    }
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table type = rawType.as<sol::table>();
        if (!isClass(type)) {
            continue;
        }
        const sol::object result = type.raw_get<sol::object>(key);
        if ((result.valid() && result.get_type() != sol::type::lua_nil) ||
            hasExplicitNilField(lua, rawType, key)) {
            if (found != nullptr) {
                *found = true;
            }
            owners.raw_set(key, type);
            return result;
        }
    }
    return nilObject(lua);
}

sol::object findClassOverride(sol::state_view lua, const sol::table& classTable,
                              const sol::object& key) {
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object value =
            rawType.as<sol::table>().raw_get<sol::object>(key);
        if ((value.valid() && value.get_type() != sol::type::lua_nil) ||
            hasExplicitNilField(lua, rawType, key)) {
            return value;
        }
    }
    return nilObject(lua);
}

bool derivesFrom(sol::state_view lua, const sol::table& classTable,
                 const sol::table& targetClass) {
    sol::object rawSet = classTable.raw_get<sol::object>("__mroSet");
    if (!rawSet.is<sol::table>()) {
        rawSet = classTable.raw_get<sol::object>("__runtimeMroSet");
    }
    if (!rawSet.is<sol::table>()) {
        getMro(lua, classTable);
        rawSet = classTable.raw_get<sol::object>("__runtimeMroSet");
    }
    if (!rawSet.is<sol::table>()) {
        return false;
    }
    const sol::object result =
        rawSet.as<sol::table>().raw_get<sol::object>(targetClass);
    return result.is<bool>() && result.as<bool>();
}

sol::table resolverMro(sol::state_view lua, const sol::table& classTable) {
    return getMro(lua, classTable);
}

}  // namespace ludork::standard::class_runtime::detail
