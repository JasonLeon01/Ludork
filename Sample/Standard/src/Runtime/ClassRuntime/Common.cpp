#include "Internal.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <climits>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::standard::class_runtime::detail {

unsigned char nativeClassDefaultResolverKeyStorage;
unsigned char nativeDeepCopyProtocolsKeyStorage;

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

namespace {

int protectedIndexThunk(lua_State* state) {
    lua_pushvalue(state, 2);
    lua_gettable(state, 1);
    return 1;
}

int protectedAssignThunk(lua_State* state) {
    lua_pushvalue(state, 2);
    lua_pushvalue(state, 3);
    lua_settable(state, 1);
    return 0;
}

}  // namespace

std::string popLuaError(lua_State* state, const char* fallback) {
    std::size_t length = 0;
    const char* message = lua_tolstring(state, -1, &length);
    const std::string result = message == nullptr
                                   ? std::string(fallback)
                                   : std::string(message, length);
    lua_pop(state, 1);
    return result;
}

sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, protectedIndexThunk);
    target.push();
    key.push();
    if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
        throw std::runtime_error(popLuaError(state, "Lua indexed read failed"));
    }
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return result;
}

void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, protectedAssignThunk);
    target.push();
    key.push();
    value.push();
    if (ludork::standard::protectedLuaCall(state, 3, 0) != LUA_OK) {
        throw std::runtime_error(
            popLuaError(state, "Lua indexed write failed"));
    }
}

bool isClass(const sol::table& value) {
    const sol::object marker = value.raw_get<sol::object>("__ludorkClass");
    return marker.is<bool>() && marker.as<bool>();
}

bool tableHasMetatable(const sol::table& value) {
    lua_State* state = value.lua_state();
    value.push();
    const bool result = lua_getmetatable(state, -1) != 0;
    lua_pop(state, result ? 2 : 1);
    return result;
}

sol::table createWeakTable(sol::state_view lua, const char* mode) {
    sol::table result = lua.create_table();
    sol::table metatable = lua.create_table();
    metatable["__mode"] = mode;
    result[sol::metatable_key] = metatable;
    return result;
}

sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode) {
    sol::table registry = lua.registry();
    const sol::object value = registry.raw_get<sol::object>(key);
    if (value.is<sol::table>()) {
        return value.as<sol::table>();
    }
    sol::table result = weakMode == nullptr ? lua.create_table()
                                            : createWeakTable(lua, weakMode);
    registry.raw_set(key, result);
    return result;
}

sol::object nativeDeepCopyProtocolsKey(sol::state_view lua) {
    return sol::make_object(lua, sol::lightuserdata_value(static_cast<void*>(
                                     &nativeDeepCopyProtocolsKeyStorage)));
}

sol::table nativeDeepCopyProtocols(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object key = nativeDeepCopyProtocolsKey(lua);
    const sol::object existing = registry.raw_get<sol::object>(key);
    if (existing.get_type() == sol::type::table) {
        return existing.as<sol::table>();
    }
    sol::table result = lua.create_table();
    registry.raw_set(key, result);
    return result;
}

std::optional<NativeDeepCopyProtocol> findNativeDeepCopyProtocol(
    sol::state_view lua, const sol::object& nativeType) {
    const sol::object rawProtocols =
        lua.registry().raw_get<sol::object>(nativeDeepCopyProtocolsKey(lua));
    if (rawProtocols.get_type() != sol::type::table) {
        return std::nullopt;
    }
    const sol::object rawProtocol =
        rawProtocols.as<sol::table>().raw_get<sol::object>(nativeType);
    if (rawProtocol.get_type() != sol::type::userdata) {
        return std::nullopt;
    }
    lua_State* state = lua.lua_state();
    rawProtocol.push();
    if (lua_rawlen(state, -1) != sizeof(NativeDeepCopyProtocol)) {
        lua_pop(state, 1);
        return std::nullopt;
    }
    const auto* protocol =
        static_cast<const NativeDeepCopyProtocol*>(lua_touserdata(state, -1));
    const auto result = *protocol;
    lua_pop(state, 1);
    return result;
}

bool tableIsEmpty(const sol::table& table) {
    for (const auto& entry : table) {
        static_cast<void>(entry);
        return false;
    }
    return true;
}

bool rawBool(const sol::table& table, const char* name) {
    const sol::object value = table.raw_get<sol::object>(name);
    return value.is<bool>() && value.as<bool>();
}

bool luaValuesEqual(sol::state_view lua, const sol::object& left,
                    const sol::object& right) {
    left.push();
    right.push();
    const bool equal = lua_compare(lua.lua_state(), -2, -1, LUA_OPEQ) != 0;
    lua_pop(lua.lua_state(), 2);
    return equal;
}

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

sol::object rawMember(sol::state_view lua, const sol::table& type,
                      const sol::object& key) {
    sol::object result = type.raw_get<sol::object>(key);
    if ((!result.valid() || result.get_type() == sol::type::lua_nil) &&
        !isClass(type)) {
        if (nativeClassProperty(lua, type, key, result)) {
            return result;
        }
        const sol::object rawIndex =
            class_native::getObjectMetatable(lua, sol::make_object(lua, type))
                .raw_get<sol::object>("__index");
        if (rawIndex.is<sol::protected_function>()) {
            sol::protected_function_result indexed =
                rawIndex.as<sol::protected_function>()(type, key);
            if (indexed.valid()) {
                result = indexed.get<sol::object>();
            }
        }
    }
    return result.valid() ? result : nilObject(lua);
}

sol::object findInClass(sol::state_view lua, const sol::table& classTable,
                        const sol::object& key, bool includeClass) {
    sol::table owners = classLookupOwners(
        lua, classTable, includeClass ? "members" : "baseMembers");
    const sol::object rawOwner = owners.raw_get<sol::object>(key);
    if (rawOwner.is<sol::table>()) {
        const sol::object cached =
            rawMember(lua, rawOwner.as<sol::table>(), key);
        if (cached.valid() && cached.get_type() != sol::type::lua_nil) {
            return cached;
        }
        owners.raw_set(key, sol::lua_nil);
    }
    const sol::table mro = getMro(lua, classTable);
    const std::size_t start = includeClass ? 1 : 2;
    for (std::size_t index = start; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object result =
            rawMember(lua, rawType.as<sol::table>(), key);
        if (result.valid() && result.get_type() != sol::type::lua_nil) {
            owners.raw_set(key, rawType);
            return result;
        }
    }
    return nilObject(lua);
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
                             const sol::object& key) {
    sol::table owners = classLookupOwners(lua, classTable, "scriptMembers");
    const sol::object rawOwner = owners.raw_get<sol::object>(key);
    if (rawOwner.is<sol::table>()) {
        const sol::object cached =
            rawOwner.as<sol::table>().raw_get<sol::object>(key);
        if (cached.valid() && cached.get_type() != sol::type::lua_nil) {
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
        if (result.valid() && result.get_type() != sol::type::lua_nil) {
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
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
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

sol::object scriptClassOf(sol::state_view lua, const sol::object& value) {
    if (value.get_type() == sol::type::table) {
        const sol::table tableValue = value.as<sol::table>();
        const sol::object rawClass = tableValue.raw_get<sol::object>("__class");
        if (rawClass.is<sol::table>()) {
            return rawClass;
        }
        const sol::table metatable =
            class_native::getObjectMetatable(lua, value);
        if (isClass(metatable)) {
            return sol::make_object(lua, metatable);
        }
    } else if (value.get_type() == sol::type::userdata) {
        lua_State* state = lua.lua_state();
        value.push();
        const int valueIndex = lua_absindex(state, -1);
        if (lua_getiuservalue(state, valueIndex, 1) == LUA_TTABLE) {
            lua_getfield(state, -1, "__class");
            const sol::object rawClass =
                sol::stack::get<sol::object>(state, -1);
            lua_pop(state, 3);
            if (rawClass.is<sol::table>()) {
                return rawClass;
            }
        } else {
            lua_pop(state, 2);
        }
    }
    return nilObject(lua);
}

sol::object typeInfoOf(sol::state_view lua, const sol::table& nativeType) {
    return class_native::getObjectMetatable(lua, sol::make_object(lua, nativeType))
        .raw_get<sol::object>("__type");
}

namespace {

sol::object findNativeTypeInNamespace(sol::state_view lua,
                                      const sol::table& nameSpace,
                                      const sol::table& targetTypeInfo) {
    for (const auto& entry : nameSpace) {
        const sol::object candidate = entry.second;
        if (!candidate.is<sol::table>()) {
            continue;
        }
        const sol::object candidateTypeInfo =
            typeInfoOf(lua, candidate.as<sol::table>());
        if (candidateTypeInfo.is<sol::table>() &&
            objectsRawEqual(candidateTypeInfo.as<sol::table>(),
                            targetTypeInfo)) {
            return candidate;
        }
    }
    return nilObject(lua);
}

}  // namespace

sol::object nativeTypeOf(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return nilObject(lua);
    }
    const sol::object rawTypeInfo =
        class_native::getObjectMetatable(lua, value)
            .raw_get<sol::object>("__type");
    if (!rawTypeInfo.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table typeInfo = rawTypeInfo.as<sol::table>();
    sol::table cache = registryTable(lua, NATIVE_TYPE_CACHE_KEY);
    const sol::object cached = cache.raw_get<sol::object>(typeInfo);
    if (cached.is<sol::table>()) {
        return cached;
    }
    const sol::table globals = lua.globals();
    sol::object result = findNativeTypeInNamespace(lua, globals, typeInfo);
    if (!result.is<sol::table>()) {
        for (const auto& entry : globals) {
            const sol::object nameSpace = entry.second;
            if (!nameSpace.is<sol::table>()) {
                continue;
            }
            const sol::table tableValue = nameSpace.as<sol::table>();
            if (objectsRawEqual(tableValue, globals)) {
                continue;
            }
            result = findNativeTypeInNamespace(lua, tableValue, typeInfo);
            if (result.is<sol::table>()) {
                break;
            }
        }
    }
    if (result.is<sol::table>()) {
        cache.raw_set(typeInfo, result);
    }
    return result;
}

sol::object actualClassOf(sol::state_view lua, const sol::object& value) {
    if (value.is<sol::table>() && isClass(value.as<sol::table>())) {
        return value;
    }
    sol::object result = scriptClassOf(lua, value);
    if (result.is<sol::table>()) {
        return result;
    }
    return nativeTypeOf(lua, value);
}

}  // namespace ludork::standard::class_runtime::detail
