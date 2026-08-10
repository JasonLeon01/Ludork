#include "ClassRuntime.hpp"

#include "ClassNativeInterop.hpp"

#include <ClassServices.hpp>
#include <LuaError.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <new>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
constexpr const char* METHOD_OWNERS_KEY = "Ludork.Class.methodOwners";
constexpr const char* NATIVE_TYPE_CACHE_KEY = "Ludork.Class.nativeTypeCache";
constexpr const char* INSTANCES_KEY = "Ludork.Class.instances";
constexpr const char* COMPOSITE_METATABLE_KEY =
    "Ludork.Class.compositeMetatable";
constexpr const char* CONSTRUCTING_COMPOSITE_METATABLE_KEY =
    "Ludork.Class.constructingCompositeMetatable";
constexpr const char* NATIVE_OWNERS_KEY = "Ludork.Class.nativeOwners";
constexpr const char* NATIVE_POINTER_OWNERS_KEY =
    "Ludork.Class.nativePointerOwners";
constexpr const char* DYNAMIC_NATIVE_WRITERS_KEY =
    "Ludork.Class.dynamicNativeWriters";
constexpr const char* SUPER_PROXY_CACHE_KEY = "Ludork.Class.superProxyCache";
constexpr const char* SUPER_PROXY_METATABLE_KEY =
    "Ludork.Class.superProxyMetatable";
constexpr const char* MONITOR_STATES_KEY = "Ludork.Class.monitorStates";
constexpr const char* RUNTIME_SERVICES_KEY = "Ludork.Class.runtimeServices";
constexpr const char* LIFECYCLE_STATES_KEY = "Ludork.Class.lifecycleStates";
constexpr const char* DISPOSED_METATABLE_KEY = "Ludork.Class.disposedMetatable";
constexpr const char* SHUTTING_DOWN_KEY = "Ludork.Class.shuttingDown";
constexpr const char* NATIVE_METHOD_CACHE_FIELD = "__nativeMethodCache";
constexpr const char* FAST_INDEX_CACHE_FIELD = "__fastIndexCache";
constexpr const char* NATIVE_INITIALIZER_FIELD = "__classInit";
constexpr const char* NATIVE_INITIALIZING_FIELD = "__initializing";
constexpr const char* NATIVE_CONSTRUCTION_FAILED_FIELD = "__constructionFailed";
constexpr const char* NATIVE_CONSTRUCTING_ROOTS_FIELD =
    "__nativeConstructingRoots";
constexpr const char* NATIVE_DIRTY_PROPERTIES_FIELD = "__nativeDirtyProperties";
constexpr const char* NATIVE_CLASS_INDEX_FIELD = "__ludorkNativeClassIndex";
constexpr const char* NATIVE_CLASS_NEW_INDEX_FIELD =
    "__ludorkNativeClassNewIndex";
constexpr const char* NATIVE_CLASS_GUARD_FIELD = "__ludorkNativeClassGuard";
constexpr const char* NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD =
    "__classResolvedDefaults";
constexpr const char* NATIVE_COPY_FIELD = "__copy";
unsigned char nativeClassDefaultResolverKeyStorage;
unsigned char nativeDeepCopyProtocolsKeyStorage;
enum class LifecycleState : lua_Integer {
    Active = 0,
    Disposing = 1,
    Disposed = 2,
};
constexpr const char* CLASS_RESERVED_FIELDS[] = {
    "__ludorkClass",
    "__name",
    "__bases",
    "__base",
    "__mro",
    "__mroSet",
    "__runtimeBases",
    "__runtimeMro",
    "__runtimeMroSet",
    "__nativeBases",
    "__nativeMro",
    "__nativeMroSet",
    "__subclasses",
    "__lookupCache",
    "__lookupVersion",
    "__index",
    "__newindex",
    "__gc",
    "__call",
    "new",
    "_hasImplementationOwner",
    "__classBaseMethods",
    "__classCallbacks",
    "__classDefaults",
    "__classResolvedDefaults",
    "__classFactory",
    "__classFactoryMinArgs",
    "__classInit",
    "__nativeMethodCache",
    "__nativeObjects",
    "__nativeProperties",
};

using ludork::standard::class_native::getObjectMetatable;
using ludork::standard::class_native::getUserFields;
using ludork::standard::class_native::nextInstanceId;

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
}

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

bool isNativeType(sol::state_view lua, const sol::table& value);
bool objectsRawEqual(const sol::object& left, const sol::object& right);
sol::object nativeObjectForType(sol::state_view lua, const sol::table& fields,
                                const sol::table& nativeType);
sol::object ensureDefaultNativeObject(sol::state_view lua,
                                      const sol::object& instance,
                                      const sol::table& nativeType);
int classInstanceGc(lua_State* state);

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
                         const char* weakMode = nullptr) {
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

std::optional<ludork::standard::class_runtime::NativeDeepCopyProtocol>
findNativeDeepCopyProtocol(sol::state_view lua, const sol::object& nativeType) {
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
    if (lua_rawlen(state, -1) !=
        sizeof(ludork::standard::class_runtime::NativeDeepCopyProtocol)) {
        lua_pop(state, 1);
        return std::nullopt;
    }
    const auto* protocol = static_cast<
        const ludork::standard::class_runtime::NativeDeepCopyProtocol*>(
        lua_touserdata(state, -1));
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

enum class MroKind {
    Runtime,
    Native
};

sol::table getMro(sol::state_view lua, sol::table type);
sol::table getNativeMro(sol::state_view lua, sol::table type);

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

bool nativeTypeDeclaresProperty(const sol::table& nativeType,
                                const sol::object& key);
bool nativeClassProperty(sol::state_view lua, const sol::table& nativeType,
                         const sol::object& key, sol::object& value);

sol::object rawMember(sol::state_view lua, const sol::table& type,
                      const sol::object& key) {
    sol::object result = type.raw_get<sol::object>(key);
    if ((!result.valid() || result.get_type() == sol::type::lua_nil) &&
        !isClass(type)) {
        if (nativeClassProperty(lua, type, key, result)) {
            return result;
        }
        const sol::object rawIndex =
            getObjectMetatable(lua, sol::make_object(lua, type))
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
                        const sol::object& key, bool includeClass = true) {
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
        const sol::table metatable = getObjectMetatable(lua, value);
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
    return getObjectMetatable(lua, sol::make_object(lua, nativeType))
        .raw_get<sol::object>("__type");
}

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

sol::object nativeTypeOf(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return nilObject(lua);
    }
    const sol::object rawTypeInfo =
        getObjectMetatable(lua, value).raw_get<sol::object>("__type");
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

bool nativeTypeAccepts(sol::state_view lua, const sol::table& nativeType,
                       const sol::object& value) {
    const sol::object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<sol::table>()) {
        return false;
    }
    const sol::object rawIs =
        rawTypeInfo.as<sol::table>().raw_get<sol::object>("is");
    if (!rawIs.is<sol::protected_function>()) {
        return false;
    }
    sol::protected_function_result result =
        rawIs.as<sol::protected_function>()(value);
    return result.valid() && result.get_type() == sol::type::boolean &&
           result.get<bool>();
}

void registerMethodOwner(sol::state_view lua, const sol::table& classTable,
                         const sol::object& value) {
    if (!value.is<sol::function>()) {
        return;
    }
    registryTable(lua, METHOD_OWNERS_KEY, "k").raw_set(value, classTable);
}

void* nativePointer(lua_State* state, int index) {
    const int absoluteIndex = lua_absindex(state, index);
    if (lua_type(state, absoluteIndex) != LUA_TUSERDATA ||
        lua_getmetatable(state, absoluteIndex) == 0) {
        return nullptr;
    }
    lua_getfield(state, -1, "__LuaSFNativeComposite");
    const bool composite = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    lua_getfield(state, -1, "__type");
    const bool native = lua_istable(state, -1);
    lua_pop(state, 2);
    if (composite || !native) {
        return nullptr;
    }
    void* memory = lua_touserdata(state, absoluteIndex);
    if (memory == nullptr) {
        return nullptr;
    }
    void* rawData = sol::detail::align_usertype_pointer(memory);
    return *static_cast<void**>(rawData);
}

void registerNativePointerOwner(sol::state_view lua,
                                const sol::object& nativeObject,
                                const sol::object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push();
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    registryTable(lua, NATIVE_POINTER_OWNERS_KEY, "v").push();
    lua_pushlightuserdata(state, pointer);
    owner.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

void unregisterNativePointerOwner(sol::state_view lua,
                                  const sol::object& nativeObject,
                                  const sol::object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push();
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    sol::table owners = registryTable(lua, NATIVE_POINTER_OWNERS_KEY, "v");
    owners.push();
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, ownersIndex);
    owner.push();
    const bool matches = lua_rawequal(state, -1, -2) != 0;
    lua_pop(state, 2);
    if (!matches) {
        lua_pop(state, 1);
        return;
    }
    lua_pushlightuserdata(state, pointer);
    lua_pushnil(state);
    lua_rawset(state, ownersIndex);
    lua_pop(state, 1);
}

bool pushNativeOwner(lua_State* state, int nativeIndex) {
    const int absoluteNativeIndex = lua_absindex(state, nativeIndex);
    sol::state_view lua(state);
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").push();
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushvalue(state, absoluteNativeIndex);
    lua_rawget(state, ownersIndex);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, ownersIndex);
        return true;
    }
    lua_pop(state, 2);
    void* pointer = nativePointer(state, absoluteNativeIndex);
    if (pointer == nullptr) {
        return false;
    }
    registryTable(lua, NATIVE_POINTER_OWNERS_KEY, "v").push();
    const int pointerOwnersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, pointerOwnersIndex);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, pointerOwnersIndex);
        return true;
    }
    lua_pop(state, 2);
    return false;
}

void restoreNativeOwners(lua_State* state) {
    const int resultCount = lua_gettop(state);
    for (int index = 1; index <= resultCount; ++index) {
        const int absoluteIndex = lua_absindex(state, index);
        if (lua_type(state, absoluteIndex) == LUA_TUSERDATA) {
            if (pushNativeOwner(state, absoluteIndex)) {
                lua_replace(state, absoluteIndex);
            }
        } else if (lua_type(state, absoluteIndex) == LUA_TTABLE) {
            const lua_Integer length =
                static_cast<lua_Integer>(lua_rawlen(state, absoluteIndex));
            for (lua_Integer itemIndex = 1; itemIndex <= length; ++itemIndex) {
                lua_rawgeti(state, absoluteIndex, itemIndex);
                const int valueIndex = lua_absindex(state, -1);
                if (lua_type(state, valueIndex) == LUA_TUSERDATA &&
                    pushNativeOwner(state, valueIndex)) {
                    lua_replace(state, valueIndex);
                }
                lua_rawseti(state, absoluteIndex, itemIndex);
            }
        }
    }
}

int boundMethodCall(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_pushvalue(state, lua_upvalueindex(2));
    lua_insert(state, 2);
    lua_call(state, argumentCount + 1, LUA_MULTRET);
    restoreNativeOwners(state);
    return lua_gettop(state);
}

sol::object bindMethod(sol::state_view lua, const sol::object& method,
                       const sol::object& self) {
    lua_State* state = lua.lua_state();
    method.push();
    self.push();
    lua_pushcclosure(state, boundMethodCall, 2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return result;
}

int nativeMethodCall(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    if (argumentCount == 0) {
        return luaL_error(state, "Native instance method requires a receiver");
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_pushvalue(state, lua_upvalueindex(2));
    lua_replace(state, 2);
    lua_call(state, argumentCount, LUA_MULTRET);
    restoreNativeOwners(state);
    return lua_gettop(state);
}

sol::object wrapNativeMethod(sol::state_view lua, const sol::object& method,
                             const sol::object& nativeObject) {
    method.push();
    nativeObject.push();
    lua_pushcclosure(lua.lua_state(), nativeMethodCall, 2);
    sol::object result = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return result;
}

bool objectsRawEqual(const sol::object& left, const sol::object& right) {
    lua_State* state = left.lua_state();
    left.push();
    right.push();
    const bool result = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    return result;
}

bool isNativeInitializer(const sol::table& nativeType,
                         const sol::object& member) {
    const sol::object initializer =
        nativeType.raw_get<sol::object>(NATIVE_INITIALIZER_FIELD);
    return initializer.is<sol::function>() && member.is<sol::function>() &&
           objectsRawEqual(initializer, member);
}

sol::object cachedBoundMethod(sol::state_view lua, sol::table proxy,
                              const sol::object& key, const sol::object& method,
                              const sol::object& receiver) {
    sol::object rawCache = proxy.raw_get<sol::object>(4);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        proxy.raw_set(4, cache);
    }
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (rawEntry.is<sol::table>()) {
        const sol::table entry = rawEntry.as<sol::table>();
        const sol::object cachedMethod = entry.raw_get<sol::object>(1);
        const sol::object cachedReceiver = entry.raw_get<sol::object>(2);
        const sol::object cachedWrapper = entry.raw_get<sol::object>(3);
        if (cachedWrapper.is<sol::function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedReceiver, receiver)) {
            return cachedWrapper;
        }
    }
    sol::table entry = lua.create_table();
    const sol::object wrapper = bindMethod(lua, method, receiver);
    entry.raw_set(1, method);
    entry.raw_set(2, receiver);
    entry.raw_set(3, wrapper);
    cache.raw_set(key, entry);
    return wrapper;
}

int superProxyIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table proxy = sol::stack::get<sol::table>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object self = proxy.raw_get<sol::object>(1);
        const sol::table actualClass = proxy.raw_get<sol::table>(2);
        const std::size_t currentIndex = proxy.raw_get<std::size_t>(3);
        const sol::table mro = getMro(lua, actualClass);
        for (std::size_t index = currentIndex + 1; index <= mro.size();
             ++index) {
            const sol::object rawType = mro.raw_get<sol::object>(index);
            if (!rawType.is<sol::table>()) {
                continue;
            }
            const sol::table type = rawType.as<sol::table>();
            const sol::object rawGetters =
                type.raw_get<sol::object>("__getters");
            if (rawGetters.is<sol::table>()) {
                const sol::object getter =
                    rawGetters.as<sol::table>().raw_get<sol::object>(key);
                if (getter.is<sol::function>()) {
                    getter.push();
                    self.push();
                    lua_call(state, 1, 1);
                    return 1;
                }
            }
            sol::object member = nilObject(lua);
            if (isNativeType(lua, type)) {
                const sol::object rawBaseMethods =
                    type.raw_get<sol::object>("__classBaseMethods");
                if (rawBaseMethods.is<sol::table>()) {
                    member =
                        rawBaseMethods.as<sol::table>().raw_get<sol::object>(
                            key);
                }
            }
            if (!member.valid() || member.get_type() == sol::type::lua_nil) {
                member = rawMember(lua, type, key);
            }
            if (!member.valid() || member.get_type() == sol::type::lua_nil) {
                continue;
            }
            if (!member.is<sol::function>()) {
                member.push();
                return 1;
            }
            sol::object receiver = self;
            if (isNativeType(lua, type) && !isNativeInitializer(type, member)) {
                const sol::table fields = getUserFields(lua, self, false);
                sol::object nativeObject =
                    nativeObjectForType(lua, fields, type);
                if (!nativeObject.is<sol::userdata>()) {
                    nativeObject = ensureDefaultNativeObject(lua, self, type);
                }
                if (nativeObject.is<sol::userdata>()) {
                    receiver = nativeObject;
                }
            }
            cachedBoundMethod(lua, proxy, key, member, receiver).push();
            return 1;
        }
        lua_pushnil(state);
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

sol::table superProxyMetatable(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable =
        registry.raw_get<sol::object>(SUPER_PROXY_METATABLE_KEY);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.push();
    lua_pushcfunction(lua.lua_state(), superProxyIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pop(lua.lua_state(), 1);
    registry.raw_set(SUPER_PROXY_METATABLE_KEY, metatable);
    return metatable;
}

sol::table createSuperProxy(sol::state_view lua, const sol::table& currentClass,
                            const sol::object& self) {
    const sol::object rawActualClass = actualClassOf(lua, self);
    if (!rawActualClass.is<sol::table>()) {
        throw std::invalid_argument("super() requires a class instance");
    }
    const sol::table actualClass = rawActualClass.as<sol::table>();
    const sol::table mro = getMro(lua, actualClass);
    std::size_t currentIndex = 0;
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (rawType.is<sol::table>() &&
            objectsRawEqual(rawType.as<sol::table>(), currentClass)) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex == 0) {
        throw std::invalid_argument(
            "super() current class is not in the instance MRO");
    }
    sol::table cache = registryTable(lua, SUPER_PROXY_CACHE_KEY, "k");
    const sol::object rawInstanceCache = cache.raw_get<sol::object>(self);
    sol::table instanceCache = rawInstanceCache.is<sol::table>()
                                   ? rawInstanceCache.as<sol::table>()
                                   : createWeakTable(lua, "v");
    if (!rawInstanceCache.is<sol::table>()) {
        cache.raw_set(self, instanceCache);
    }
    const sol::object rawProxy =
        instanceCache.raw_get<sol::object>(currentClass);
    if (rawProxy.is<sol::table>()) {
        return rawProxy.as<sol::table>();
    }
    sol::table proxy = lua.create_table();
    proxy.raw_set(1, self);
    proxy.raw_set(2, actualClass);
    proxy.raw_set(3, currentIndex);
    proxy[sol::metatable_key] = superProxyMetatable(lua);
    instanceCache.raw_set(currentClass, proxy);
    return proxy;
}

bool inferSuperContext(lua_State* state, sol::table& currentClass,
                       sol::object& self) {
    lua_Debug record{};
    if (lua_getstack(state, 1, &record) == 0 ||
        lua_getinfo(state, "f", &record) == 0) {
        return false;
    }
    sol::state_view lua(state);
    const sol::object caller = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    const sol::object rawOwner =
        registryTable(lua, METHOD_OWNERS_KEY, "k").raw_get<sol::object>(caller);
    if (!rawOwner.is<sol::table>()) {
        return false;
    }
    currentClass = rawOwner.as<sol::table>();
    if (lua_getlocal(state, &record, 1) == nullptr) {
        return false;
    }
    self = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return true;
}

int superFunction(lua_State* state) {
    sol::state_view lua(state);
    const int argumentCount = lua_gettop(state);
    if (argumentCount == 2) {
        if (!lua_istable(state, 1)) {
            return luaL_error(state, "super() first argument must be a class");
        }
        createSuperProxy(lua, sol::stack::get<sol::table>(state, 1),
                         sol::stack::get<sol::object>(state, 2))
            .push();
        return 1;
    }
    if (argumentCount != 0 && argumentCount != 1) {
        return luaL_error(state, "super() expects zero, one, or two arguments");
    }
    sol::table currentClass = lua.create_table();
    sol::object inferredSelf = nilObject(lua);
    if (!inferSuperContext(state, currentClass, inferredSelf)) {
        return luaL_error(state,
                          "super() could not determine the defining class");
    }
    const sol::object self = argumentCount == 1
                                 ? sol::stack::get<sol::object>(state, 1)
                                 : inferredSelf;
    createSuperProxy(lua, currentClass, self).push();
    return 1;
}

bool isNativeType(sol::state_view lua, const sol::table& value) {
    return !isClass(value) && typeInfoOf(lua, value).is<sol::table>();
}

std::string nativeTypeName(sol::state_view lua, const sol::table& nativeType) {
    const sol::object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<sol::table>()) {
        throw std::invalid_argument(
            "Native class is missing binding type information");
    }
    const sol::object rawName =
        rawTypeInfo.as<sol::table>().raw_get<sol::object>("name");
    if (!rawName.is<std::string>()) {
        throw std::invalid_argument(
            "Native class is missing its qualified type name");
    }
    return rawName.as<std::string>();
}

sol::object nativeTypeDefinition(sol::state_view lua,
                                 const sol::table& nativeType,
                                 const sol::object& key) {
    const std::string registryName = "sol." + nativeTypeName(lua, nativeType);
    const sol::object rawMetatable =
        lua.registry().raw_get<sol::object>(registryName);
    if (!rawMetatable.is<sol::table>()) {
        return nilObject(lua);
    }
    return rawMetatable.as<sol::table>().raw_get<sol::object>(key);
}

bool nativeTypeDeclaresProperty(const sol::table& nativeType,
                                const sol::object& key) {
    if (!key.is<std::string>()) {
        return false;
    }
    const sol::object rawProperties =
        nativeType.raw_get<sol::object>("__nativeProperties");
    if (!rawProperties.is<sol::table>()) {
        return false;
    }
    const std::string name = key.as<std::string>();
    const sol::table properties = rawProperties.as<sol::table>();
    for (std::size_t index = 1; index <= properties.size(); ++index) {
        const sol::object rawName = properties.raw_get<sol::object>(index);
        if (rawName.is<std::string>() && rawName.as<std::string>() == name) {
            return true;
        }
    }
    return false;
}

sol::object nativeClassDefaultResolverKey(sol::state_view lua) {
    return sol::make_object(
        lua, sol::lightuserdata_value(
                 static_cast<void*>(&nativeClassDefaultResolverKeyStorage)));
}

sol::object resolveNativeClassDefault(sol::state_view lua,
                                      sol::table nativeType,
                                      const sol::object& key,
                                      sol::table defaults,
                                      const sol::object& value) {
    const sol::object rawResolvedDefaults =
        nativeType.raw_get<sol::object>(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD);
    sol::table resolvedDefaults = rawResolvedDefaults.is<sol::table>()
                                      ? rawResolvedDefaults.as<sol::table>()
                                      : lua.create_table();
    if (!rawResolvedDefaults.is<sol::table>()) {
        nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                           resolvedDefaults);
    }
    const sol::object rawResolved =
        resolvedDefaults.raw_get<sol::object>(key);
    if (rawResolved.is<bool>() && rawResolved.as<bool>()) {
        return value;
    }

    const sol::object rawMetadata =
        nativeType.raw_get<sol::object>("__runtimeMetadata");
    if (!rawMetadata.is<sol::table>()) {
        return value;
    }
    const sol::object rawFieldMetadata =
        rawMetadata.as<sol::table>().raw_get<sol::object>(key);
    if (!rawFieldMetadata.is<sol::table>()) {
        return value;
    }
    const sol::object rawResolver = lua.registry().raw_get<sol::object>(
        nativeClassDefaultResolverKey(lua));
    if (!rawResolver.is<sol::protected_function>()) {
        return value;
    }
    sol::protected_function resolver =
        rawResolver.as<sol::protected_function>();
    const sol::object rawModule =
        rawMetadata.as<sol::table>().raw_get<sol::object>("module");
    sol::protected_function_result result =
        resolver(value, rawFieldMetadata.as<sol::table>(), rawModule);
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    if (result.return_count() == 0) {
        throw std::runtime_error(
            "Native class default resolver returned no value");
    }
    const sol::object resolved = result.get<sol::object>();
    defaults.raw_set(key, resolved);
    resolvedDefaults.raw_set(key, true);
    return resolved;
}

bool nativeClassProperty(sol::state_view lua, const sol::table& nativeType,
                         const sol::object& key, sol::object& value) {
    const sol::table mro = getMro(lua, nativeType);
    sol::object rawOverride = nilObject(lua);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table current = rawType.as<sol::table>();
        if (rawOverride.get_type() == sol::type::lua_nil) {
            const sol::object rawValue = current.raw_get<sol::object>(key);
            if (rawValue.valid() && rawValue.get_type() != sol::type::lua_nil) {
                rawOverride = rawValue;
            }
        }
        if (!nativeTypeDeclaresProperty(current, key)) {
            continue;
        }
        if (rawOverride.get_type() != sol::type::lua_nil) {
            value = rawOverride;
            return true;
        }
        const sol::object rawDefaults =
            current.raw_get<sol::object>("__classDefaults");
        if (rawDefaults.is<sol::table>()) {
            sol::table defaults = rawDefaults.as<sol::table>();
            value = defaults.raw_get<sol::object>(key);
            if (value.valid() && value.get_type() != sol::type::lua_nil) {
                value = resolveNativeClassDefault(lua, current, key, defaults,
                                                  value);
            }
        } else {
            value = nilObject(lua);
        }
        if (!value.valid()) {
            value = nilObject(lua);
        }
        return true;
    }
    return false;
}

sol::object nativeClassIndex(sol::table nativeType, sol::object key,
                             sol::this_state state) {
    sol::state_view lua(state);
    sol::object value = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, value)) {
        return value;
    }
    const sol::table metatable =
        getObjectMetatable(lua, sol::make_object(lua, nativeType));
    const sol::object original =
        metatable.raw_get<sol::object>(NATIVE_CLASS_INDEX_FIELD);
    if (original.is<sol::protected_function>()) {
        sol::protected_function_result result =
            original.as<sol::protected_function>()(nativeType, key);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return result.return_count() == 0 ? nilObject(lua)
                                          : result.get<sol::object>();
    }
    if (original.is<sol::table>()) {
        return protectedIndex(lua, original, key);
    }
    return nilObject(lua);
}

void nativeClassNewIndex(sol::table nativeType, sol::object key,
                         sol::object value, sol::this_state state) {
    sol::state_view lua(state);
    sol::object ignored = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, ignored)) {
        nativeType.raw_set(key, value);
        invalidateClassLookup(lua, nativeType);
        return;
    }
    const sol::table metatable =
        getObjectMetatable(lua, sol::make_object(lua, nativeType));
    const sol::object original =
        metatable.raw_get<sol::object>(NATIVE_CLASS_NEW_INDEX_FIELD);
    if (original.is<sol::protected_function>()) {
        sol::protected_function_result result =
            original.as<sol::protected_function>()(nativeType, key, value);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return;
    }
    if (original.is<sol::table>()) {
        original.as<sol::table>().raw_set(key, value);
        return;
    }
    nativeType.raw_set(key, value);
}

bool nativeFallbackMemberEligible(const sol::object& key) {
    if (!key.is<std::string>()) {
        return true;
    }
    const std::string name = key.as<std::string>();
    return !name.starts_with("__") && name != "class_check" &&
           name != "class_cast";
}

std::vector<sol::table> nativeRoots(sol::state_view lua,
                                    const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        bool covered = false;
        for (const sol::table& root : result) {
            if (derivesFrom(lua, root, nativeType)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            result.push_back(nativeType);
        }
    }
    return result;
}

sol::object nativeObjectForType(sol::state_view lua, const sol::table& fields,
                                const sol::table& nativeType) {
    const sol::object rawObjects =
        fields.raw_get<sol::object>("__nativeObjects");
    if (!rawObjects.is<sol::table>()) {
        return nilObject(lua);
    }
    return rawObjects.as<sol::table>().raw_get<sol::object>(
        nativeTypeName(lua, nativeType));
}

sol::object cachedNativeMethod(sol::state_view lua, sol::table fields,
                               const sol::object& key,
                               const sol::object& method,
                               const sol::object& nativeObject,
                               const sol::table& nativeType,
                               bool objectMember) {
    const sol::object rawCache =
        fields.raw_get<sol::object>(NATIVE_METHOD_CACHE_FIELD);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        fields.raw_set(NATIVE_METHOD_CACHE_FIELD, cache);
    }
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (rawEntry.is<sol::table>()) {
        const sol::table entry = rawEntry.as<sol::table>();
        const sol::object cachedMethod = entry.raw_get<sol::object>(1);
        const sol::object cachedObject = entry.raw_get<sol::object>(2);
        const sol::object cachedWrapper = entry.raw_get<sol::object>(3);
        if (cachedWrapper.is<sol::function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedObject, nativeObject)) {
            return cachedWrapper;
        }
    }
    sol::table entry = lua.create_table();
    const sol::object wrapper = wrapNativeMethod(lua, method, nativeObject);
    entry.raw_set(1, method);
    entry.raw_set(2, nativeObject);
    entry.raw_set(3, wrapper);
    entry.raw_set(4, nativeType);
    entry.raw_set(5, objectMember);
    cache.raw_set(key, entry);
    return wrapper;
}

sol::object findCachedNativeMethod(sol::state_view lua, sol::table fields,
                                   const sol::object& key) {
    const sol::object rawCache =
        fields.raw_get<sol::object>(NATIVE_METHOD_CACHE_FIELD);
    if (!rawCache.is<sol::table>()) {
        return nilObject(lua);
    }
    sol::table cache = rawCache.as<sol::table>();
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (!rawEntry.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table entry = rawEntry.as<sol::table>();
    const sol::object method = entry.raw_get<sol::object>(1);
    const sol::object nativeObject = entry.raw_get<sol::object>(2);
    const sol::object wrapper = entry.raw_get<sol::object>(3);
    const sol::object rawNativeType = entry.raw_get<sol::object>(4);
    const sol::object rawObjectMember = entry.raw_get<sol::object>(5);
    if (!method.is<sol::function>() || !nativeObject.is<sol::userdata>() ||
        !wrapper.is<sol::function>() || !rawNativeType.is<sol::table>() ||
        !rawObjectMember.is<bool>()) {
        cache.raw_set(key, sol::lua_nil);
        return nilObject(lua);
    }
    sol::object current = nilObject(lua);
    if (rawObjectMember.as<bool>()) {
        current = protectedIndex(lua, nativeObject, key);
    } else {
        current = rawMember(lua, rawNativeType.as<sol::table>(), key);
    }
    if (current.is<sol::function>() && objectsRawEqual(current, method)) {
        return wrapper;
    }
    cache.raw_set(key, sol::lua_nil);
    return nilObject(lua);
}

enum class FastIndexKind : lua_Integer {
    Value = 1,
    ScriptMember = 2,
    Getter = 3,
    NativeMember = 4,
};

lua_Integer classLookupVersion(const sol::table& classTable) {
    const sol::object rawVersion =
        classTable.raw_get<sol::object>("__lookupVersion");
    return rawVersion.is<lua_Integer>() ? rawVersion.as<lua_Integer>() : 0;
}

sol::table fastIndexCache(sol::state_view lua, sol::table fields) {
    const sol::object rawCache =
        fields.raw_get<sol::object>(FAST_INDEX_CACHE_FIELD);
    if (rawCache.is<sol::table>()) {
        return rawCache.as<sol::table>();
    }
    sol::table cache = lua.create_table();
    fields.raw_set(FAST_INDEX_CACHE_FIELD, cache);
    return cache;
}

void cacheFastIndex(sol::state_view lua, sol::table fields,
                    const sol::table& classTable, const sol::object& key,
                    FastIndexKind kind, const sol::object& route) {
    sol::table entry = lua.create_table(3, 0);
    entry.raw_set(1, static_cast<lua_Integer>(kind));
    entry.raw_set(2, route);
    entry.raw_set(3, classLookupVersion(classTable));
    fastIndexCache(lua, fields).raw_set(key, entry);
}

void cacheFastClassOwner(sol::state_view lua, sol::table fields,
                         const sol::table& classTable, const sol::object& key,
                         const char* category, FastIndexKind kind) {
    const sol::object rawOwner =
        classLookupOwners(lua, classTable, category).raw_get<sol::object>(key);
    if (rawOwner.is<sol::table>()) {
        cacheFastIndex(lua, fields, classTable, key, kind, rawOwner);
    }
}

sol::object compositeIndexSlow(sol::object target, sol::object key,
                               sol::this_state state) {
    sol::state_view lua(state);
    const sol::table fields = getUserFields(lua, target, false);
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const sol::object field = fields.raw_get<sol::object>(key);
    if (field.valid() && field.get_type() != sol::type::lua_nil) {
        return field;
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table classTable = rawClass.as<sol::table>();
    const sol::object getter = findAccessor(lua, classTable, "__getters", key);
    if (getter.is<sol::function>()) {
        cacheFastClassOwner(lua, fields, classTable, key, "__getters",
                            FastIndexKind::Getter);
        return getter.as<sol::function>()(target);
    }
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        const sol::object nativeDefinition =
            nativeTypeDefinition(lua, nativeType, key);
        if (!nativeDefinition.valid() ||
            nativeDefinition.get_type() == sol::type::lua_nil) {
            continue;
        }
        if (!declaredProperty && !nativeDefinition.is<sol::function>()) {
            continue;
        }
        sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
        if (!nativeObject.is<sol::userdata>() && declaredProperty) {
            nativeObject = ensureDefaultNativeObject(lua, target, nativeType);
        }
        if (!nativeObject.is<sol::userdata>()) {
            continue;
        }
        const sol::object nativeValue = protectedIndex(lua, nativeObject, key);
        if (declaredProperty || !nativeValue.is<sol::function>()) {
            cacheFastIndex(
                lua, fields, classTable, key, FastIndexKind::NativeMember,
                sol::make_object(lua, nativeTypeName(lua, nativeType)));
            return nativeValue;
        }
    }
    const sol::object scriptMember = findScriptMember(lua, classTable, key);
    if (scriptMember.valid() && scriptMember.get_type() != sol::type::lua_nil) {
        cacheFastClassOwner(lua, fields, classTable, key, "scriptMembers",
                            FastIndexKind::ScriptMember);
        return scriptMember;
    }
    const sol::object cachedNative = findCachedNativeMethod(lua, fields, key);
    if (cachedNative.is<sol::function>()) {
        cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                       cachedNative);
        return cachedNative;
    }
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
        sol::object nativeValue = nilObject(lua);
        if (nativeObject.is<sol::userdata>()) {
            const sol::object nativeDefinition =
                nativeTypeDefinition(lua, nativeType, key);
            if (nativeDefinition.valid() &&
                nativeDefinition.get_type() != sol::type::lua_nil &&
                (declaredProperty || nativeDefinition.is<sol::function>())) {
                nativeValue = protectedIndex(lua, nativeObject, key);
                if (!nativeValue.is<sol::function>()) {
                    return nativeValue;
                }
            }
        }
        const sol::object member = rawMember(lua, nativeType, key);
        if (member.valid() && member.get_type() != sol::type::lua_nil) {
            if (isNativeInitializer(nativeType, member)) {
                const sol::object wrapper = cachedNativeMethod(
                    lua, fields, key, member, target, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            if (member.is<sol::function>()) {
                if (!nativeObject.is<sol::userdata>()) {
                    nativeObject =
                        ensureDefaultNativeObject(lua, target, nativeType);
                }
                const sol::object wrapper = cachedNativeMethod(
                    lua, fields, key, member, nativeObject, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            return member;
        }
        if (nativeValue.is<sol::function>()) {
            const sol::object wrapper = cachedNativeMethod(
                lua, fields, key, nativeValue, nativeObject, nativeType, true);
            cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                           wrapper);
            return wrapper;
        }
    }
    return nilObject(lua);
}

int returnTopValue(lua_State* state) {
    lua_replace(state, 1);
    lua_settop(state, 1);
    return 1;
}

void invalidateFastIndexEntry(lua_State* state, int cacheIndex) {
    lua_pushvalue(state, 2);
    lua_pushnil(state);
    lua_rawset(state, cacheIndex);
}

int compositeIndex(lua_State* state) {
    try {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);
            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool constructionFailed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (constructionFailed) {
                return luaL_error(state, "Class instance construction failed");
            }

            lua_pushvalue(state, 2);
            lua_rawget(state, fieldsIndex);
            if (!lua_isnil(state, -1)) {
                return returnTopValue(state);
            }
            lua_pop(state, 1);

            lua_getfield(state, fieldsIndex, FAST_INDEX_CACHE_FIELD);
            if (lua_istable(state, -1)) {
                const int cacheIndex = lua_absindex(state, -1);
                lua_pushvalue(state, 2);
                lua_rawget(state, cacheIndex);
                if (lua_istable(state, -1)) {
                    const int entryIndex = lua_absindex(state, -1);
                    lua_getfield(state, fieldsIndex, "__class");
                    if (lua_istable(state, -1)) {
                        lua_getfield(state, -1, "__lookupVersion");
                        const lua_Integer currentVersion =
                            lua_isinteger(state, -1) ? lua_tointeger(state, -1)
                                                     : 0;
                        lua_pop(state, 2);
                        lua_rawgeti(state, entryIndex, 3);
                        const lua_Integer cachedVersion =
                            lua_isinteger(state, -1) ? lua_tointeger(state, -1)
                                                     : -1;
                        lua_pop(state, 1);
                        if (currentVersion == cachedVersion) {
                            lua_rawgeti(state, entryIndex, 1);
                            const FastIndexKind kind =
                                static_cast<FastIndexKind>(
                                    lua_isinteger(state, -1)
                                        ? lua_tointeger(state, -1)
                                        : 0);
                            lua_pop(state, 1);
                            if (kind == FastIndexKind::Value) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (!lua_isnil(state, -1)) {
                                    return returnTopValue(state);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::ScriptMember) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_pushvalue(state, 2);
                                    lua_rawget(state, -2);
                                    if (!lua_isnil(state, -1)) {
                                        return returnTopValue(state);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::Getter) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_getfield(state, -1, "__getters");
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            lua_call(state, 1, 1);
                                            return returnTopValue(state);
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::NativeMember) {
                                lua_getfield(state, fieldsIndex,
                                             "__nativeObjects");
                                if (lua_istable(state, -1)) {
                                    lua_rawgeti(state, entryIndex, 2);
                                    lua_rawget(state, -2);
                                    if (lua_isuserdata(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_gettable(state, -2);
                                        if (!lua_isnil(state, -1)) {
                                            return returnTopValue(state);
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            }
                        }
                    } else {
                        lua_pop(state, 1);
                    }
                    invalidateFastIndexEntry(state, cacheIndex);
                }
            }
        }
        lua_settop(state, 2);
        sol::state_view lua(state);
        const sol::object target = sol::stack::get<sol::object>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object result = compositeIndexSlow(target, key, state);
        result.push();
        return returnTopValue(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

bool setNativeObjectMember(sol::state_view lua, const sol::object& nativeObject,
                           const sol::table& nativeType, const sol::object& key,
                           const sol::object& value) {
    if (!nativeObject.is<sol::userdata>()) {
        return false;
    }
    const bool declaredProperty = nativeTypeDeclaresProperty(nativeType, key);
    if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
        return false;
    }
    const sol::object nativeDefinition =
        nativeTypeDefinition(lua, nativeType, key);
    if (!nativeDefinition.valid() ||
        nativeDefinition.get_type() == sol::type::lua_nil) {
        return false;
    }
    if (!declaredProperty && !nativeDefinition.is<sol::function>()) {
        return false;
    }
    const sol::object current = protectedIndex(lua, nativeObject, key);
    if (!declaredProperty && current.is<sol::function>()) {
        return false;
    }
    protectedAssign(lua, nativeObject, key, value);
    return true;
}

bool setNativeMember(sol::state_view lua, const sol::table& fields,
                     const sol::table& classTable, const sol::object& key,
                     const sol::object& value,
                     sol::object* assignedObject = nullptr) {
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const sol::object nativeObject =
            nativeObjectForType(lua, fields, nativeType);
        if (setNativeObjectMember(lua, nativeObject, nativeType, key, value)) {
            if (assignedObject != nullptr) {
                *assignedObject = nativeObject;
            }
            return true;
        }
    }
    return false;
}

void markNativePropertyDirty(sol::state_view lua, sol::table fields,
                             const sol::object& nativeObject,
                             const sol::object& key) {
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return;
    }
    const sol::object rawDirty =
        fields.raw_get<sol::object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    sol::table dirty = rawDirty.is<sol::table>() ? rawDirty.as<sol::table>()
                                                 : lua.create_table();
    if (!rawDirty.is<sol::table>()) {
        fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, dirty);
    }
    const sol::object rawProperties = dirty.raw_get<sol::object>(nativeObject);
    sol::table properties = rawProperties.is<sol::table>()
                                ? rawProperties.as<sol::table>()
                                : lua.create_table();
    if (!rawProperties.is<sol::table>()) {
        dirty.raw_set(nativeObject, properties);
    }
    properties.raw_set(key, true);
}

using NativeShadowSnapshot = std::vector<std::pair<sol::object, sol::object>>;

void restoreNativeShadows(sol::table fields,
                          const NativeShadowSnapshot& snapshot) {
    for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend();
         ++iterator) {
        fields.raw_set(iterator->first, iterator->second);
    }
}

void syncNativeRootDefaults(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance, const sol::table& root,
                            const sol::object& nativeObject,
                            NativeShadowSnapshot& shadowSnapshot) {
    sol::table fields = getUserFields(lua, instance, false);
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object rawProperties =
            rawType.as<sol::table>().raw_get<sol::object>("__nativeProperties");
        if (!rawProperties.is<sol::table>()) {
            continue;
        }
        const sol::table nativeProperties = rawProperties.as<sol::table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const sol::object rawName =
                nativeProperties.raw_get<sol::object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    const sol::table classMro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= classMro.size(); ++index) {
        const sol::object rawType = classMro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>() || !isClass(rawType.as<sol::table>())) {
            continue;
        }
        for (const auto& entry : rawType.as<sol::table>()) {
            if (entry.first.is<std::string>() &&
                nativeFallbackMemberEligible(entry.first)) {
                const std::string name = entry.first.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const sol::object key = sol::make_object(lua, property);
        const sol::object shadow = fields.raw_get<sol::object>(key);
        const bool hasShadow =
            shadow.valid() && shadow.get_type() != sol::type::lua_nil;
        const sol::object value =
            hasShadow ? shadow : findClassOverride(lua, classTable, key);
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            continue;
        }
        bool assigned = false;
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const sol::object rawType = nativeMro.raw_get<sol::object>(index);
            if (!rawType.is<sol::table>()) {
                continue;
            }
            if (setNativeObjectMember(lua, nativeObject,
                                      rawType.as<sol::table>(), key, value)) {
                assigned = true;
                break;
            }
        }
        if (assigned && hasShadow) {
            shadowSnapshot.emplace_back(key, shadow);
            fields.raw_set(key, sol::lua_nil);
        }
    }
}

void replayNativeDirtyProperties(sol::state_view lua, const sol::table& fields,
                                 const sol::table& root,
                                 const sol::object& source,
                                 const sol::object& destination) {
    if (!source.is<sol::userdata>() || !destination.is<sol::userdata>()) {
        return;
    }
    const sol::object rawDirty =
        fields.raw_get<sol::object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    if (!rawDirty.is<sol::table>()) {
        return;
    }
    const sol::object rawProperties =
        rawDirty.as<sol::table>().raw_get<sol::object>(source);
    if (!rawProperties.is<sol::table>()) {
        return;
    }
    const sol::table nativeMro = getNativeMro(lua, root);
    for (const auto& entry : rawProperties.as<sol::table>()) {
        if (!entry.second.is<bool>() || !entry.second.as<bool>()) {
            continue;
        }
        const sol::object key = entry.first;
        const sol::object value = protectedIndex(lua, source, key);
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const sol::object rawType = nativeMro.raw_get<sol::object>(index);
            if (rawType.is<sol::table>() &&
                setNativeObjectMember(lua, destination,
                                      rawType.as<sol::table>(), key, value)) {
                break;
            }
        }
    }
}

void syncNativeClassDefaults(sol::state_view lua, const sol::table& classTable,
                             const sol::object& instance) {
    if (instance.get_type() != sol::type::userdata) {
        return;
    }
    const sol::table fields = getUserFields(lua, instance, false);
    if (!fields.raw_get<sol::object>("__nativeObjects").is<sol::table>()) {
        return;
    }
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const sol::object rawProperties =
            nativeType.raw_get<sol::object>("__nativeProperties");
        if (!rawProperties.is<sol::table>()) {
            continue;
        }
        const sol::table nativeProperties = rawProperties.as<sol::table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const sol::object rawName =
                nativeProperties.raw_get<sol::object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>() || !isClass(rawType.as<sol::table>())) {
            continue;
        }
        for (const auto& entry : rawType.as<sol::table>()) {
            if (entry.first.is<std::string>() &&
                nativeFallbackMemberEligible(entry.first)) {
                const std::string name = entry.first.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const sol::object key = sol::make_object(lua, property);
        const sol::object value = findClassOverride(lua, classTable, key);
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            setNativeMember(lua, fields, classTable, key, value);
        }
    }
}

sol::object monitorMissing(sol::state_view lua) {
    const sol::object rawClass = lua.globals().raw_get<sol::object>("Class");
    if (rawClass.is<sol::table>()) {
        sol::table classModule = rawClass.as<sol::table>();
        const sol::object missing = classModule.raw_get<sol::object>("MISSING");
        if (missing.is<sol::table>()) {
            return missing;
        }
        sol::table created = lua.create_table();
        classModule.raw_set("MISSING", created);
        return created;
    }
    return lua.create_table();
}

sol::table monitorState(sol::state_view lua, const sol::object& target) {
    const sol::object rawState = registryTable(lua, MONITOR_STATES_KEY, "k")
                                     .raw_get<sol::object>(target);
    return rawState.is<sol::table>() ? rawState.as<sol::table>()
                                     : lua.create_table();
}

sol::object originalMonitoredIndex(sol::state_view lua, const sol::table& state,
                                   const sol::object& target,
                                   const sol::object& key) {
    const sol::object originalIndex = state.raw_get<sol::object>("index");
    if (originalIndex.is<sol::protected_function>()) {
        sol::protected_function_result result =
            originalIndex.as<sol::protected_function>()(target, key);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return result.get<sol::object>();
    }
    if (originalIndex.is<sol::table>()) {
        return originalIndex.as<sol::table>().raw_get<sol::object>(key);
    }
    return target.as<sol::table>().raw_get<sol::object>(key);
}

void originalMonitoredNewIndex(sol::state_view lua, const sol::table& state,
                               const sol::object& target,
                               const sol::object& key,
                               const sol::object& value) {
    const sol::object originalNewIndex = state.raw_get<sol::object>("newIndex");
    if (originalNewIndex.is<sol::protected_function>()) {
        sol::protected_function_result result =
            originalNewIndex.as<sol::protected_function>()(target, key, value);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return;
    }
    if (originalNewIndex.is<sol::table>()) {
        originalNewIndex.as<sol::table>().raw_set(key, value);
        return;
    }
    target.as<sol::table>().raw_set(key, value);
}

void invokeMonitorCallback(sol::state_view lua, sol::table entry,
                           const sol::object& oldValue,
                           const sol::object& newValue) {
    if (luaValuesEqual(lua, oldValue, newValue)) {
        return;
    }
    const sol::object rawRunning = entry.raw_get<sol::object>("running");
    if (rawRunning.is<bool>() && rawRunning.as<bool>()) {
        return;
    }
    const sol::object rawCallback = entry.raw_get<sol::object>("callback");
    if (!rawCallback.is<sol::protected_function>()) {
        return;
    }
    entry.raw_set("running", true);
    rawCallback.push();
    oldValue.push();
    newValue.push();
    int argumentCount = 2;
    const sol::object rawParams = entry.raw_get<sol::object>("params");
    if (rawParams.is<sol::table>()) {
        const sol::table params = rawParams.as<sol::table>();
        for (std::size_t index = 1; index <= params.size(); ++index) {
            params.raw_get<sol::object>(index).push();
            ++argumentCount;
        }
    }
    if (ludork::standard::protectedLuaCall(lua.lua_state(), argumentCount, 0) !=
        LUA_OK) {
        const char* message = lua_tostring(lua.lua_state(), -1);
        const std::string error =
            message == nullptr ? "Monitor callback failed" : message;
        lua_pop(lua.lua_state(), 1);
        entry.raw_set("running", false);
        throw std::runtime_error(error);
    }
    entry.raw_set("running", false);
}

sol::object monitoredTableIndex(sol::object target, sol::object key,
                                sol::this_state state) {
    sol::state_view lua(state);
    const sol::table monitor = monitorState(lua, target);
    const sol::object rawFields = monitor.raw_get<sol::object>("fields");
    if (rawFields.is<sol::table>()) {
        const sol::object rawEntry =
            rawFields.as<sol::table>().raw_get<sol::object>(key);
        if (rawEntry.is<sol::table>()) {
            const sol::table entry = rawEntry.as<sol::table>();
            const sol::object rawHasValue =
                entry.raw_get<sol::object>("hasValue");
            if (rawHasValue.is<bool>() && rawHasValue.as<bool>()) {
                return entry.raw_get<sol::object>("value");
            }
            return nilObject(lua);
        }
    }
    return originalMonitoredIndex(lua, monitor, target, key);
}

void monitoredTableNewIndex(sol::object target, sol::object key,
                            sol::object value, sol::this_state state) {
    sol::state_view lua(state);
    const sol::table monitor = monitorState(lua, target);
    const sol::object rawFields = monitor.raw_get<sol::object>("fields");
    const sol::object rawEntry =
        rawFields.is<sol::table>()
            ? rawFields.as<sol::table>().raw_get<sol::object>(key)
            : nilObject(lua);
    if (!rawEntry.is<sol::table>()) {
        originalMonitoredNewIndex(lua, monitor, target, key, value);
        return;
    }
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    sol::table entry = rawEntry.as<sol::table>();
    const sol::object rawHasValue = entry.raw_get<sol::object>("hasValue");
    const sol::object oldValue =
        rawHasValue.is<bool>() && rawHasValue.as<bool>()
            ? entry.raw_get<sol::object>("value")
            : entry.raw_get<sol::object>("missing");
    entry.raw_set("value", value);
    entry.raw_set("hasValue", true);
    entry.raw_set("assigned", true);
    invokeMonitorCallback(lua, entry, oldValue, value);
}

sol::table createTableMonitorState(sol::state_view lua, sol::table target) {
    lua_State* state = lua.lua_state();
    target.push();
    sol::object originalMetatable = nilObject(lua);
    if (lua_getmetatable(state, -1) != 0) {
        originalMetatable = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    if (originalMetatable.is<sol::table>()) {
        const sol::object protection =
            originalMetatable.as<sol::table>().raw_get<sol::object>(
                "__metatable");
        if (protection.valid() && protection.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "Lua monitors cannot replace a protected metatable");
        }
    }
    sol::table monitor = lua.create_table();
    sol::table fields = lua.create_table();
    monitor.raw_set("meta", originalMetatable);
    monitor.raw_set("fields", fields);
    if (originalMetatable.is<sol::table>()) {
        sol::table original = originalMetatable.as<sol::table>();
        monitor.raw_set("index", original.raw_get<sol::object>("__index"));
        monitor.raw_set("newIndex",
                        original.raw_get<sol::object>("__newindex"));
    }
    sol::table proxy = lua.create_table();
    if (originalMetatable.is<sol::table>()) {
        for (const auto& entry : originalMetatable.as<sol::table>()) {
            proxy.raw_set(entry.first, entry.second);
        }
    }
    proxy.set_function("__index", &monitoredTableIndex);
    proxy.set_function("__newindex", &monitoredTableNewIndex);
    target.push();
    proxy.push();
    lua_setmetatable(state, -2);
    lua_pop(state, 1);
    registryTable(lua, MONITOR_STATES_KEY, "k").raw_set(target, monitor);
    return monitor;
}

void registerMonitor(sol::this_state state, const sol::object& target,
                     const std::string& name,
                     const sol::protected_function& callback,
                     sol::optional<sol::table> params) {
    sol::state_view lua(state);
    if (name.empty()) {
        throw std::invalid_argument("Monitor field name must not be empty");
    }
    if (target.get_type() == sol::type::table) {
        sol::table object = target.as<sol::table>();
        sol::table monitor = monitorState(lua, target);
        if (!monitor.raw_get<sol::object>("fields").is<sol::table>()) {
            monitor = createTableMonitorState(lua, object);
        }
        sol::table fields = monitor.raw_get<sol::table>("fields");
        const sol::object rawEntry = fields.raw_get<sol::object>(name);
        if (rawEntry.is<sol::table>()) {
            sol::table entry = rawEntry.as<sol::table>();
            entry.raw_set("callback", callback);
            entry.raw_set("params", params.value_or(lua.create_table()));
            return;
        }
        const sol::object rawValue = object.raw_get<sol::object>(name);
        sol::object value = rawValue;
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            value = originalMonitoredIndex(lua, monitor, target,
                                           sol::make_object(lua, name));
        }
        sol::table entry = lua.create_table();
        const bool hasValue =
            value.valid() && value.get_type() != sol::type::lua_nil;
        entry.raw_set("hasValue", hasValue);
        if (hasValue) {
            entry.raw_set("value", value);
        }
        entry.raw_set("callback", callback);
        entry.raw_set("params", params.value_or(lua.create_table()));
        entry.raw_set("running", false);
        entry.raw_set("raw", rawValue.valid() &&
                                 rawValue.get_type() != sol::type::lua_nil);
        entry.raw_set("assigned", false);
        entry.raw_set("missing", monitorMissing(lua));
        fields.raw_set(name, entry);
        object.raw_set(name, sol::lua_nil);
        return;
    }
    if (target.get_type() != sol::type::userdata) {
        throw std::invalid_argument(
            "Monitors require a table or userdata target");
    }
    sol::table fields = getUserFields(lua, target, true);
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    sol::table callbacks = rawCallbacks.is<sol::table>()
                               ? rawCallbacks.as<sol::table>()
                               : lua.create_table();
    if (!rawCallbacks.is<sol::table>()) {
        fields.raw_set("__monitorCallbacks", callbacks);
    }
    sol::table entry = lua.create_table();
    entry.raw_set("callback", callback);
    entry.raw_set("params", params.value_or(lua.create_table()));
    entry.raw_set("running", false);
    entry.raw_set("missing", monitorMissing(lua));
    callbacks.raw_set(name, entry);
}

void unregisterMonitor(sol::this_state state, const sol::object& target,
                       const std::string& name) {
    sol::state_view lua(state);
    if (target.get_type() == sol::type::table) {
        sol::table object = target.as<sol::table>();
        sol::table monitor = monitorState(lua, target);
        const sol::object rawFields = monitor.raw_get<sol::object>("fields");
        if (!rawFields.is<sol::table>()) {
            return;
        }
        sol::table fields = rawFields.as<sol::table>();
        const sol::object rawEntry = fields.raw_get<sol::object>(name);
        if (!rawEntry.is<sol::table>()) {
            return;
        }
        sol::table entry = rawEntry.as<sol::table>();
        fields.raw_set(name, sol::lua_nil);
        const bool restore =
            rawBool(entry, "raw") || rawBool(entry, "assigned");
        if (restore) {
            const bool hasValue = rawBool(entry, "hasValue");
            object.raw_set(name, hasValue ? entry.raw_get<sol::object>("value")
                                          : nilObject(lua));
        }
        if (!tableIsEmpty(fields)) {
            return;
        }
        const sol::object originalMetatable =
            monitor.raw_get<sol::object>("meta");
        object.push();
        if (originalMetatable.valid() &&
            originalMetatable.get_type() != sol::type::lua_nil) {
            originalMetatable.push();
        } else {
            lua_pushnil(lua.lua_state());
        }
        lua_setmetatable(lua.lua_state(), -2);
        lua_pop(lua.lua_state(), 1);
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_set(object, sol::lua_nil);
        return;
    }
    if (target.get_type() != sol::type::userdata) {
        return;
    }
    sol::table fields = getUserFields(lua, target, true);
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    if (!rawCallbacks.is<sol::table>()) {
        return;
    }
    sol::table callbacks = rawCallbacks.as<sol::table>();
    callbacks.raw_set(name, sol::lua_nil);
    if (tableIsEmpty(callbacks)) {
        fields.raw_set("__monitorCallbacks", sol::lua_nil);
    }
}

void compositeNewIndex(sol::object target, sol::object key, sol::object value,
                       sol::this_state state) {
    sol::state_view lua(state);
    sol::table fields = getUserFields(lua, target, true);
    const sol::object rawFastIndexCache =
        fields.raw_get<sol::object>(FAST_INDEX_CACHE_FIELD);
    if (rawFastIndexCache.is<sol::table>()) {
        rawFastIndexCache.as<sol::table>().raw_set(key, sol::lua_nil);
    }
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    auto assignValue = [&]() {
        if (!rawClass.is<sol::table>()) {
            fields.raw_set(key, value);
            return;
        }
        const sol::table classTable = rawClass.as<sol::table>();
        const sol::object setter =
            findAccessor(lua, classTable, "__setters", key);
        if (setter.is<sol::function>()) {
            setter.as<sol::function>()(target, value);
            return;
        }
        sol::object assignedObject = nilObject(lua);
        if (setNativeMember(lua, fields, classTable, key, value,
                            &assignedObject)) {
            markNativePropertyDirty(lua, fields, assignedObject, key);
            return;
        }
        fields.raw_set(key, value);
    };
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    if (!rawCallbacks.is<sol::table>()) {
        assignValue();
        return;
    }
    const sol::object rawEntry =
        rawCallbacks.as<sol::table>().raw_get<sol::object>(key);
    if (!rawEntry.is<sol::table>()) {
        assignValue();
        return;
    }
    sol::table entry = rawEntry.as<sol::table>();
    const sol::object rawRunning = entry.raw_get<sol::object>("running");
    if (rawRunning.is<bool>() && rawRunning.as<bool>()) {
        assignValue();
        return;
    }
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    sol::object oldValue = compositeIndexSlow(target, key, state);
    if (!oldValue.valid() || oldValue.get_type() == sol::type::lua_nil) {
        oldValue = entry.raw_get<sol::object>("missing");
    }
    assignValue();
    invokeMonitorCallback(lua, entry, oldValue, value);
}

sol::table createCompositeMetatable(sol::state_view lua, const char* key,
                                    bool finalized) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable = registry.raw_get<sol::object>(key);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.raw_set("__LuaSFNativeComposite", true);
    metatable.push();
    lua_pushcfunction(lua.lua_state(), compositeIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pop(lua.lua_state(), 1);
    metatable.set_function("__newindex", &compositeNewIndex);
    if (finalized) {
        metatable.push();
        lua_pushcfunction(lua.lua_state(), classInstanceGc);
        lua_setfield(lua.lua_state(), -2, "__gc");
        lua_pop(lua.lua_state(), 1);
    }
    registry.raw_set(key, metatable);
    return metatable;
}

sol::table compositeMetatable(sol::state_view lua) {
    return createCompositeMetatable(lua, COMPOSITE_METATABLE_KEY, true);
}

sol::table constructingCompositeMetatable(sol::state_view lua) {
    return createCompositeMetatable(lua, CONSTRUCTING_COMPOSITE_METATABLE_KEY,
                                    false);
}

bool pushNativeCallbackInstance(lua_State* state) {
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushnil(state);
    if (lua_next(state, -2) == 0) {
        lua_pop(state, 1);
        lua_pushnil(state);
        return false;
    }
    lua_pop(state, 1);
    lua_remove(state, -2);
    return true;
}

int nativeCallback(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    const char* methodName = lua_tostring(state, lua_upvalueindex(2));
    if (!pushNativeCallbackInstance(state)) {
        lua_pop(state, 1);
        return luaL_error(state,
                          "Native callback owner for '%s' has been disposed",
                          methodName);
    }
    sol::state_view lua(state);
    const sol::object instance = sol::stack::get<sol::object>(state, -1);
    const sol::object rawClass = actualClassOf(lua, instance);
    const sol::object callback =
        rawClass.is<sol::table>()
            ? findScriptMember(lua, rawClass.as<sol::table>(),
                               sol::make_object(lua, methodName))
            : nilObject(lua);
    if (!callback.is<sol::function>()) {
        return luaL_error(state, "Native callback method '%s' is not defined",
                          methodName);
    }
    callback.push();
    lua_insert(state, 1);
    lua_insert(state, 2);
    if (ludork::standard::protectedLuaCall(state, argumentCount + 1,
                                           LUA_MULTRET) != LUA_OK) {
        const std::string message =
            ludork::standard::luaErrorMessage(state, -1);
        return luaL_error(state, "Native callback '%s' failed: %s", methodName,
                          message.c_str());
    }
    return lua_gettop(state);
}

int nativeSelf(lua_State* state) {
    pushNativeCallbackInstance(state);
    return 1;
}

sol::table nativeCallbacks(sol::state_view lua, const sol::table& nativeType,
                           const sol::object& instance) {
    sol::table result = lua.create_table();
    sol::table holder = createWeakTable(lua, "k");
    holder.raw_set(instance, true);
    holder.push();
    lua_pushcclosure(lua.lua_state(), nativeSelf, 1);
    sol::object selfResolver =
        sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    result.raw_set("__self", selfResolver);
    const sol::object rawNames =
        rawMember(lua, nativeType, sol::make_object(lua, "__classCallbacks"));
    if (!rawNames.is<sol::table>()) {
        return result;
    }
    const sol::table names = rawNames.as<sol::table>();
    const sol::object rawClass = actualClassOf(lua, instance);
    for (std::size_t index = 1; index <= names.size(); ++index) {
        const sol::object rawName = names[index];
        if (!rawName.is<std::string>()) {
            continue;
        }
        const std::string name = rawName.as<std::string>();
        if (!rawClass.is<sol::table>() ||
            !findScriptMember(lua, rawClass.as<sol::table>(), rawName)
                 .is<sol::function>()) {
            continue;
        }
        holder.push();
        lua_pushlstring(lua.lua_state(), name.c_str(), name.size());
        lua_pushcclosure(lua.lua_state(), nativeCallback, 2);
        sol::object callback =
            sol::stack::get<sol::object>(lua.lua_state(), -1);
        lua_pop(lua.lua_state(), 1);
        result.raw_set(name, callback);
    }
    return result;
}

sol::object invokeNativeFactory(sol::state_view lua,
                                const sol::table& nativeType,
                                const sol::object& instance,
                                const sol::object& rawArguments) {
    sol::object rawFactory =
        rawMember(lua, nativeType, sol::make_object(lua, "__classFactory"));
    const bool isClassFactory = rawFactory.is<sol::protected_function>();
    if (!isClassFactory) {
        rawFactory = rawMember(lua, nativeType, sol::make_object(lua, "new"));
    }
    if (!rawFactory.is<sol::protected_function>()) {
        throw std::runtime_error("Native base " +
                                 nativeTypeName(lua, nativeType) +
                                 " has no class factory");
    }
    lua_State* state = lua.lua_state();
    rawFactory.push();
    int argumentCount = 0;
    if (isClassFactory) {
        nativeCallbacks(lua, nativeType, instance).push();
        ++argumentCount;
    }
    if (rawArguments.valid() && rawArguments.get_type() != sol::type::lua_nil) {
        if (!rawArguments.is<sol::table>()) {
            throw std::invalid_argument("Native constructor arguments for " +
                                        nativeTypeName(lua, nativeType) +
                                        " must be a packed table");
        }
        const sol::table arguments = rawArguments.as<sol::table>();
        const sol::object rawCount = arguments.raw_get<sol::object>("n");
        const lua_Integer count =
            rawCount.is<lua_Integer>()
                ? rawCount.as<lua_Integer>()
                : static_cast<lua_Integer>(arguments.size());
        if (count < 0) {
            throw std::invalid_argument(
                "Native constructor argument count cannot be negative");
        }
        for (lua_Integer index = 1; index <= count; ++index) {
            arguments.raw_get<sol::object>(index).push();
            ++argumentCount;
        }
    }
    if (ludork::standard::protectedLuaCall(state, argumentCount, 1) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        const std::string error =
            message == nullptr ? "Native class factory failed" : message;
        lua_pop(state, 1);
        throw std::runtime_error(error);
    }
    sol::object nativeObject = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    if (nativeObject.get_type() != sol::type::userdata) {
        throw std::runtime_error("Native class factory for " +
                                 nativeTypeName(lua, nativeType) +
                                 " did not return userdata");
    }
    return nativeObject;
}

void validateNativeObject(sol::state_view lua, const sol::table& root,
                          const sol::object& nativeObject) {
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!nativeTypeAccepts(lua, nativeType, nativeObject)) {
            throw std::runtime_error(
                "Native factory for " + nativeTypeName(lua, root) +
                " does not implement " + nativeTypeName(lua, nativeType));
        }
    }
}

void addNativeObject(sol::state_view lua, sol::table nativeObjects,
                     const sol::table& root, const sol::object& nativeObject) {
    validateNativeObject(lua, root, nativeObject);
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        const std::string name = nativeTypeName(lua, nativeType);
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (!current.valid() || current.get_type() == sol::type::lua_nil) {
            nativeObjects.raw_set(name, nativeObject);
        }
    }
}

void replaceNativeObject(sol::state_view lua, sol::table nativeObjects,
                         const sol::table& root, const sol::object& previous,
                         const sol::object& nativeObject) {
    validateNativeObject(lua, root, nativeObject);
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const std::string name = nativeTypeName(lua, rawType.as<sol::table>());
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (!current.valid() || current.get_type() == sol::type::lua_nil ||
            objectsRawEqual(current, previous)) {
            nativeObjects.raw_set(name, nativeObject);
        }
    }
}

void removeNativeObject(sol::state_view lua, sol::table nativeObjects,
                        const sol::table& root,
                        const sol::object& nativeObject) {
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const std::string name = nativeTypeName(lua, rawType.as<sol::table>());
        const sol::object current = nativeObjects.raw_get<sol::object>(name);
        if (objectsRawEqual(current, nativeObject)) {
            nativeObjects.raw_set(name, sol::lua_nil);
        }
    }
}

bool isCompositeInstance(sol::state_view lua, const sol::object& instance) {
    if (!instance.is<sol::userdata>()) {
        return false;
    }
    const sol::object marker =
        getObjectMetatable(lua, instance)
            .raw_get<sol::object>("__LuaSFNativeComposite");
    return marker.is<bool>() && marker.as<bool>();
}

bool nativeRootIsDeferred(const sol::table& root) {
    const sol::object rawMinimum =
        root.raw_get<sol::object>("__classFactoryMinArgs");
    return rawMinimum.is<lua_Integer>() && rawMinimum.as<lua_Integer>() >= 0;
}

void registerNativeOwner(sol::state_view lua, const sol::object& nativeObject,
                         const sol::object& instance) {
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").raw_set(nativeObject, instance);
    registerNativePointerOwner(lua, nativeObject, instance);
}

void unregisterNativeOwner(sol::state_view lua, const sol::object& nativeObject,
                           const sol::object& owner) {
    sol::table owners = registryTable(lua, NATIVE_OWNERS_KEY, "kv");
    const sol::object current = owners.raw_get<sol::object>(nativeObject);
    if (!objectsRawEqual(current, owner)) {
        return;
    }
    owners.raw_set(nativeObject, sol::lua_nil);
    unregisterNativePointerOwner(lua, nativeObject, owner);
}

void clearNativeMethodCaches(sol::state_view lua, const sol::object& instance,
                             sol::table fields) {
    fields.raw_set(NATIVE_METHOD_CACHE_FIELD, sol::lua_nil);
    registryTable(lua, SUPER_PROXY_CACHE_KEY, "k")
        .raw_set(instance, sol::lua_nil);
}

sol::table nativeRootsUnderConstruction(sol::state_view lua,
                                        sol::table fields) {
    const sol::object rawRoots =
        fields.raw_get<sol::object>(NATIVE_CONSTRUCTING_ROOTS_FIELD);
    if (rawRoots.is<sol::table>()) {
        return rawRoots.as<sol::table>();
    }
    sol::table roots = lua.create_table();
    fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, roots);
    return roots;
}

void beginNativeRootConstruction(sol::state_view lua, sol::table fields,
                                 const sol::table& root) {
    sol::table roots = nativeRootsUnderConstruction(lua, fields);
    const sol::object active = roots.raw_get<sol::object>(root);
    if (active.is<bool>() && active.as<bool>()) {
        throw std::runtime_error("Recursive native root construction for " +
                                 nativeTypeName(lua, root));
    }
    roots.raw_set(root, true);
}

void endNativeRootConstruction(sol::table fields, const sol::table& root) {
    const sol::object rawRoots =
        fields.raw_get<sol::object>(NATIVE_CONSTRUCTING_ROOTS_FIELD);
    if (!rawRoots.is<sol::table>()) {
        return;
    }
    sol::table roots = rawRoots.as<sol::table>();
    roots.raw_set(root, sol::lua_nil);
    if (tableIsEmpty(roots)) {
        fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, sol::lua_nil);
    }
}

sol::object constructNativeRoot(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance,
                                const sol::table& root,
                                const sol::object& arguments) {
    sol::table fields = getUserFields(lua, instance, false);
    const sol::object rawObjects =
        fields.raw_get<sol::object>("__nativeObjects");
    const sol::object rawInstanceId =
        fields.raw_get<sol::object>("__instanceId");
    if (!rawObjects.is<sol::table>() || !rawInstanceId.is<std::size_t>()) {
        throw std::runtime_error(
            "Composite instance has incomplete native state");
    }
    sol::table nativeObjects = rawObjects.as<sol::table>();
    beginNativeRootConstruction(lua, fields, root);
    sol::object nativeObject = nilObject(lua);
    NativeShadowSnapshot shadowSnapshot;
    try {
        nativeObject = invokeNativeFactory(lua, root, instance, arguments);
        addNativeObject(lua, nativeObjects, root, nativeObject);
        registerNativeOwner(lua, nativeObject, instance);
        syncNativeRootDefaults(lua, classTable, instance, root, nativeObject,
                               shadowSnapshot);
    } catch (...) {
        if (nativeObject.is<sol::userdata>()) {
            removeNativeObject(lua, nativeObjects, root, nativeObject);
            unregisterNativeOwner(lua, nativeObject, instance);
        }
        restoreNativeShadows(fields, shadowSnapshot);
        endNativeRootConstruction(fields, root);
        clearNativeMethodCaches(lua, instance, fields);
        throw;
    }
    endNativeRootConstruction(fields, root);
    clearNativeMethodCaches(lua, instance, fields);
    return nativeObject;
}

int nativeBaseInitializer(lua_State* state) {
    try {
        sol::state_view lua(state);
        if (lua_gettop(state) < 1) {
            throw std::invalid_argument(
                "Native base initializer requires a class instance");
        }
        const sol::table nativeType =
            sol::stack::get<sol::table>(state, lua_upvalueindex(1));
        const sol::object instance = sol::stack::get<sol::object>(state, 1);
        if (!isCompositeInstance(lua, instance)) {
            throw std::invalid_argument(
                "Native base initializer requires a composite class instance");
        }
        sol::table fields = getUserFields(lua, instance, false);
        const sol::object rawInitializing =
            fields.raw_get<sol::object>(NATIVE_INITIALIZING_FIELD);
        if (!rawInitializing.is<bool>() || !rawInitializing.as<bool>()) {
            throw std::runtime_error(
                "Native base initializers may only run during class "
                "construction");
        }
        const sol::object rawClass = fields.raw_get<sol::object>("__class");
        if (!rawClass.is<sol::table>()) {
            throw std::runtime_error("Composite instance has no class");
        }
        const sol::table classTable = rawClass.as<sol::table>();
        bool knownRoot = false;
        for (const sol::table& root : nativeRoots(lua, classTable)) {
            if (objectsRawEqual(root, nativeType)) {
                knownRoot = true;
                break;
            }
        }
        if (!knownRoot) {
            throw std::invalid_argument(
                "Native initializer target is not an exact class root");
        }
        const sol::object rawInitialized =
            fields.raw_get<sol::object>("__classInitializedRoots");
        sol::table initialized = rawInitialized.is<sol::table>()
                                     ? rawInitialized.as<sol::table>()
                                     : lua.create_table();
        if (!rawInitialized.is<sol::table>()) {
            fields.raw_set("__classInitializedRoots", initialized);
        }
        const sol::object alreadyInitialized =
            initialized.raw_get<sol::object>(nativeType);
        if (alreadyInitialized.is<bool>() && alreadyInitialized.as<bool>()) {
            throw std::runtime_error(
                "Native base initializer was called twice for " +
                nativeTypeName(lua, nativeType));
        }
        const sol::object rawObjects =
            fields.raw_get<sol::object>("__nativeObjects");
        if (!rawObjects.is<sol::table>()) {
            throw std::runtime_error(
                "Composite instance has no native object map");
        }
        sol::table nativeObjects = rawObjects.as<sol::table>();
        const sol::object previous =
            nativeObjects.raw_get<sol::object>(nativeTypeName(lua, nativeType));
        const int argumentCount = lua_gettop(state) - 1;
        if (previous.is<sol::userdata>() && argumentCount == 0) {
            initialized.raw_set(nativeType, true);
            return 0;
        }
        sol::table arguments = lua.create_table();
        arguments.raw_set("n", argumentCount);
        for (int index = 0; index < argumentCount; ++index) {
            arguments.raw_set(index + 1,
                              sol::stack::get<sol::object>(state, index + 2));
        }
        if (!previous.is<sol::userdata>()) {
            constructNativeRoot(lua, classTable, instance, nativeType,
                                arguments);
            initialized.raw_set(nativeType, true);
            return 0;
        }
        const sol::object rawInstanceId =
            fields.raw_get<sol::object>("__instanceId");
        if (!rawInstanceId.is<std::size_t>()) {
            throw std::runtime_error(
                "Composite instance has no native instance id");
        }
        beginNativeRootConstruction(lua, fields, nativeType);
        sol::object nativeObject = nilObject(lua);
        NativeShadowSnapshot shadowSnapshot;
        bool replaced = false;
        try {
            nativeObject =
                invokeNativeFactory(lua, nativeType, instance, arguments);
            validateNativeObject(lua, nativeType, nativeObject);
            replaceNativeObject(lua, nativeObjects, nativeType, previous,
                                nativeObject);
            replaced = true;
            registerNativeOwner(lua, nativeObject, instance);
            syncNativeRootDefaults(lua, classTable, instance, nativeType,
                                   nativeObject, shadowSnapshot);
            replayNativeDirtyProperties(lua, fields, nativeType, previous,
                                        nativeObject);
        } catch (...) {
            if (replaced) {
                replaceNativeObject(lua, nativeObjects, nativeType,
                                    nativeObject, previous);
                unregisterNativeOwner(lua, nativeObject, instance);
                registerNativeOwner(lua, previous, instance);
            }
            restoreNativeShadows(fields, shadowSnapshot);
            endNativeRootConstruction(fields, nativeType);
            clearNativeMethodCaches(lua, instance, fields);
            throw;
        }
        unregisterNativeOwner(lua, previous, instance);
        registerNativeOwner(lua, nativeObject, instance);
        endNativeRootConstruction(fields, nativeType);
        clearNativeMethodCaches(lua, instance, fields);
        initialized.raw_set(nativeType, true);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

void ensureNativeInitializer(sol::state_view lua, sol::table nativeType) {
    const sol::object rawFactory =
        rawMember(lua, nativeType, sol::make_object(lua, "__classFactory"));
    const sol::object rawNew =
        rawMember(lua, nativeType, sol::make_object(lua, "new"));
    if (!rawFactory.is<sol::protected_function>() &&
        !rawNew.is<sol::protected_function>()) {
        return;
    }
    sol::object initializer =
        nativeType.raw_get<sol::object>(NATIVE_INITIALIZER_FIELD);
    if (!initializer.is<sol::function>()) {
        nativeType.push();
        lua_pushcclosure(lua.lua_state(), nativeBaseInitializer, 1);
        initializer = sol::stack::get<sol::object>(lua.lua_state(), -1);
        lua_pop(lua.lua_state(), 1);
        nativeType.raw_set(NATIVE_INITIALIZER_FIELD, initializer);
    }
    const sol::object existing =
        rawMember(lua, nativeType, sol::make_object(lua, "init"));
    if (!existing.valid() || existing.get_type() == sol::type::lua_nil) {
        nativeType.raw_set("init", initializer);
    }
}

void validateNativeInstanceShape(sol::state_view lua,
                                 const sol::table& classTable,
                                 const sol::object& instance) {
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    if (!isCompositeInstance(lua, instance)) {
        throw std::runtime_error(
            "Class with native bases must return its composite instance");
    }
    const sol::table fields = getUserFields(lua, instance, false);
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>() ||
        !objectsRawEqual(rawClass.as<sol::table>(), classTable)) {
        throw std::runtime_error("Composite instance belongs to another class");
    }
}

void validateNativeRoots(sol::state_view lua, const sol::table& classTable,
                         const sol::object& instance) {
    validateNativeInstanceShape(lua, classTable, instance);
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    const sol::table fields = getUserFields(lua, instance, false);
    for (const sol::table& root : roots) {
        if (!nativeObjectForType(lua, fields, root).is<sol::userdata>()) {
            throw std::runtime_error("Lua class initializer must call " +
                                     nativeTypeName(lua, root) +
                                     ".init(self, ...)");
        }
    }
}

bool compositeBelongsToClass(sol::state_view lua, const sol::object& instance,
                             const sol::table& classTable) {
    if (!isCompositeInstance(lua, instance)) {
        return false;
    }
    const sol::object rawClass =
        getUserFields(lua, instance, false).raw_get<sol::object>("__class");
    return rawClass.is<sol::table>() &&
           objectsRawEqual(rawClass.as<sol::table>(), classTable);
}

bool tableMatchesInstanceClass(sol::state_view lua, const sol::table& instance,
                               const sol::table& classTable) {
    if (objectsRawEqual(instance, classTable)) {
        return false;
    }
    const sol::table metatable =
        getObjectMetatable(lua, sol::make_object(lua, instance));
    if (objectsRawEqual(metatable, classTable)) {
        return true;
    }
    const sol::object rawMonitor = registryTable(lua, MONITOR_STATES_KEY, "k")
                                       .raw_get<sol::object>(instance);
    if (!rawMonitor.is<sol::table>()) {
        return false;
    }
    const sol::object originalMetatable =
        rawMonitor.as<sol::table>().raw_get<sol::object>("meta");
    return originalMetatable.is<sol::table>() &&
           objectsRawEqual(originalMetatable.as<sol::table>(), classTable);
}

std::optional<sol::table> tryManagedInstanceFields(
    sol::state_view lua, const sol::object& instance) {
    if (isCompositeInstance(lua, instance)) {
        const sol::table fields = getUserFields(lua, instance, false);
        const sol::object rawClass = fields.raw_get<sol::object>("__class");
        if (rawClass.is<sol::table>() && isClass(rawClass.as<sol::table>())) {
            return fields;
        }
    } else if (instance.is<sol::table>()) {
        const sol::table fields = instance.as<sol::table>();
        const sol::object rawClass = fields.raw_get<sol::object>("__class");
        if (rawClass.is<sol::table>() && isClass(rawClass.as<sol::table>()) &&
            tableMatchesInstanceClass(lua, fields, rawClass.as<sol::table>())) {
            return fields;
        }
    }
    return std::nullopt;
}

sol::table managedInstanceFields(sol::state_view lua,
                                 const sol::object& instance) {
    const std::optional<sol::table> fields =
        tryManagedInstanceFields(lua, instance);
    if (fields.has_value()) {
        return *fields;
    }
    throw std::invalid_argument(
        "Class lifecycle requires a Ludork class instance");
}

LifecycleState lifecycleState(sol::state_view lua,
                              const sol::object& instance) {
    const sol::object rawState = registryTable(lua, LIFECYCLE_STATES_KEY, "k")
                                     .raw_get<sol::object>(instance);
    if (rawState.is<lua_Integer>()) {
        const lua_Integer value = rawState.as<lua_Integer>();
        if (value == static_cast<lua_Integer>(LifecycleState::Disposing)) {
            return LifecycleState::Disposing;
        }
        if (value == static_cast<lua_Integer>(LifecycleState::Disposed)) {
            return LifecycleState::Disposed;
        }
    }
    return LifecycleState::Active;
}

void setLifecycleState(sol::state_view lua, const sol::object& instance,
                       LifecycleState state) {
    registryTable(lua, LIFECYCLE_STATES_KEY, "k")
        .raw_set(instance, static_cast<lua_Integer>(state));
}

int disposedInstanceAccess(lua_State* state) {
    return luaL_error(state, "Class instance has been disposed");
}

sol::table disposedInstanceMetatable(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable =
        registry.raw_get<sol::object>(DISPOSED_METATABLE_KEY);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.push();
    lua_pushcfunction(lua.lua_state(), disposedInstanceAccess);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pushcfunction(lua.lua_state(), disposedInstanceAccess);
    lua_setfield(lua.lua_state(), -2, "__newindex");
    lua_pop(lua.lua_state(), 1);
    metatable.raw_set("__metatable", "disposed");
    registry.raw_set(DISPOSED_METATABLE_KEY, metatable);
    return metatable;
}

void protectDisposedInstance(sol::state_view lua, const sol::object& instance) {
    instance.push();
    disposedInstanceMetatable(lua).push();
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
}

void reportDisposeError(const char* phase, const std::string& message) {
    std::fprintf(stderr, "Class dispose %s failed: %s\n", phase,
                 message.c_str());
}

template <typename Callback>
void runDisposePhase(const char* phase, Callback&& callback) noexcept {
    try {
        callback();
    } catch (const std::exception& error) {
        reportDisposeError(phase, error.what());
    } catch (...) {
        reportDisposeError(phase, "unknown error");
    }
}

void invokeClassDispose(sol::state_view lua, const sol::object& instance,
                        const sol::table& classTable) {
    const sol::object dispose =
        findScriptMember(lua, classTable, sol::make_object(lua, "dispose"));
    if (!dispose.is<sol::function>()) {
        return;
    }
    dispose.push();
    instance.push();
    if (ludork::standard::protectedLuaCall(lua.lua_state(), 1, 0) != LUA_OK) {
        reportDisposeError("dispose", popLuaError(lua.lua_state(),
                                                  "Lua class dispose failed"));
    }
}

void clearInstanceMonitor(sol::state_view lua, const sol::object& instance) {
    sol::table states = registryTable(lua, MONITOR_STATES_KEY, "k");
    const sol::object rawMonitor = states.raw_get<sol::object>(instance);
    if (!instance.is<sol::table>() || !rawMonitor.is<sol::table>()) {
        states.raw_set(instance, sol::lua_nil);
        return;
    }
    const sol::object originalMetatable =
        rawMonitor.as<sol::table>().raw_get<sol::object>("meta");
    instance.push();
    if (originalMetatable.valid() &&
        originalMetatable.get_type() != sol::type::lua_nil) {
        originalMetatable.push();
    } else {
        lua_pushnil(lua.lua_state());
    }
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
    states.raw_set(instance, sol::lua_nil);
}

struct NativeDisposeTarget {
    sol::table root;
    sol::object nativeObject;
    bool requiresHook{};
};

struct DisposeSnapshot {
    sol::table fields;
    sol::table classTable;
    std::optional<std::size_t> instanceId;
    std::vector<NativeDisposeTarget> nativeTargets;
};

DisposeSnapshot createDisposeSnapshot(sol::state_view lua,
                                      const sol::object& instance) {
    const sol::table fields = managedInstanceFields(lua, instance);
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>()) {
        throw std::runtime_error("Class instance has no runtime class");
    }
    DisposeSnapshot snapshot{
        fields,
        rawClass.as<sol::table>(),
        std::nullopt,
        {},
    };
    const sol::object rawInstanceId =
        fields.raw_get<sol::object>("__instanceId");
    if (rawInstanceId.is<std::size_t>()) {
        snapshot.instanceId = rawInstanceId.as<std::size_t>();
    }
    for (const sol::table& root : nativeRoots(lua, snapshot.classTable)) {
        const sol::object nativeObject = nativeObjectForType(lua, fields, root);
        if (!nativeObject.is<sol::userdata>()) {
            continue;
        }
        const sol::object rawCallbacks =
            root.raw_get<sol::object>("__classCallbacks");
        const sol::object rawMetadataModule =
            root.raw_get<sol::object>("__metadataModule");
        snapshot.nativeTargets.push_back({
            root,
            nativeObject,
            rawCallbacks.is<sol::table>() &&
                !tableIsEmpty(rawCallbacks.as<sol::table>()) &&
                rawMetadataModule.is<std::string>(),
        });
    }
    return snapshot;
}

void invokeNativeDisposeHooks(sol::state_view lua, const sol::object& instance,
                              const std::vector<NativeDisposeTarget>& targets) {
    for (const NativeDisposeTarget& target : targets) {
        runDisposePhase("native hook", [&]() {
            const sol::object rawDispose = rawMember(
                lua, target.root, sol::make_object(lua, "__classRelease"));
            if (rawDispose.is<sol::protected_function>()) {
                sol::protected_function_result disposed =
                    rawDispose.as<sol::protected_function>()(
                        target.nativeObject);
                if (!disposed.valid()) {
                    const sol::error error = disposed;
                    reportDisposeError("native hook", error.what());
                }
            } else if (target.requiresHook) {
                reportDisposeError("native hook",
                                   "Missing __classRelease for " +
                                       nativeTypeName(lua, target.root));
            }
        });
        runDisposePhase("native owner", [&]() {
            unregisterNativeOwner(lua, target.nativeObject, instance);
        });
    }
}

void clearInstanceFields(sol::table fields) {
    std::vector<sol::object> keys;
    for (const auto& entry : fields) {
        keys.push_back(entry.first);
    }
    for (const sol::object& key : keys) {
        fields.raw_set(key, sol::lua_nil);
    }
}

bool disposeInstanceCore(sol::state_view lua, const sol::object& instance,
                         bool invokeDispose) {
    if (lifecycleState(lua, instance) != LifecycleState::Active) {
        return false;
    }

    DisposeSnapshot snapshot = createDisposeSnapshot(lua, instance);
    setLifecycleState(lua, instance, LifecycleState::Disposing);
    if (invokeDispose) {
        runDisposePhase("dispose", [&]() {
            invokeClassDispose(lua, instance, snapshot.classTable);
        });
    }

    runDisposePhase("monitor", [&]() {
        clearInstanceMonitor(lua, instance);
    });
    runDisposePhase("native roots", [&]() {
        invokeNativeDisposeHooks(lua, instance, snapshot.nativeTargets);
    });
    runDisposePhase("instance registry", [&]() {
        if (snapshot.instanceId.has_value()) {
            registryTable(lua, INSTANCES_KEY, "v")
                .raw_set(*snapshot.instanceId, sol::lua_nil);
        }
    });
    runDisposePhase("method cache", [&]() {
        clearNativeMethodCaches(lua, instance, snapshot.fields);
    });
    runDisposePhase("instance fields", [&]() {
        clearInstanceFields(snapshot.fields);
    });
    runDisposePhase("disposed metatable", [&]() {
        protectDisposedInstance(lua, instance);
    });
    setLifecycleState(lua, instance, LifecycleState::Disposed);
    return true;
}

int classInstanceGc(lua_State* state) {
    try {
        if (lua_gettop(state) >= 1) {
            sol::state_view lua(state);
            const sol::object shuttingDown =
                lua.registry().raw_get<sol::object>(SHUTTING_DOWN_KEY);
            if (!(shuttingDown.is<bool>() && shuttingDown.as<bool>())) {
                disposeInstanceCore(lua, sol::stack::get<sol::object>(state, 1),
                                    true);
            }
        }
    } catch (const std::exception& error) {
        reportDisposeError("__gc", error.what());
    } catch (...) {
        reportDisposeError("__gc", "unknown error");
    }
    return 0;
}

void failNativeConstruction(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance) {
    const sol::object rawClass = scriptClassOf(lua, instance);
    if (!rawClass.is<sol::table>() ||
        !objectsRawEqual(rawClass.as<sol::table>(), classTable)) {
        return;
    }
    runDisposePhase("failed construction", [&]() {
        disposeInstanceCore(lua, instance, false);
    });
}

void finishNativeConstruction(sol::state_view lua, const sol::table& classTable,
                              const sol::object& instance) {
    if (!compositeBelongsToClass(lua, instance, classTable)) {
        return;
    }
    sol::table fields = getUserFields(lua, instance, false);
    fields.raw_set(NATIVE_INITIALIZING_FIELD, false);
    fields.raw_set(NATIVE_CONSTRUCTION_FAILED_FIELD, sol::lua_nil);
    fields.raw_set("__classInitializedRoots", sol::lua_nil);
    fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, sol::lua_nil);
    fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, sol::lua_nil);
    instance.push();
    compositeMetatable(lua).push();
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
}

void completeDefaultNativeRoots(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance) {
    if (!isCompositeInstance(lua, instance)) {
        return;
    }
    sol::table fields = getUserFields(lua, instance, false);
    for (const sol::table& root : nativeRoots(lua, classTable)) {
        if (nativeObjectForType(lua, fields, root).is<sol::userdata>()) {
            continue;
        }
        const sol::object rawMinimum =
            root.raw_get<sol::object>("__classFactoryMinArgs");
        if (!rawMinimum.is<lua_Integer>() ||
            rawMinimum.as<lua_Integer>() != 0) {
            continue;
        }
        constructNativeRoot(lua, classTable, instance, root, nilObject(lua));
    }
}

sol::object ensureDefaultNativeObject(sol::state_view lua,
                                      const sol::object& instance,
                                      const sol::table& nativeType) {
    if (!isCompositeInstance(lua, instance)) {
        return nilObject(lua);
    }
    sol::table fields = getUserFields(lua, instance, false);
    sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
    if (nativeObject.is<sol::userdata>()) {
        return nativeObject;
    }
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return nilObject(lua);
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    const sol::object rawObjects =
        fields.raw_get<sol::object>("__nativeObjects");
    const sol::object rawInstanceId =
        fields.raw_get<sol::object>("__instanceId");
    if (!rawClass.is<sol::table>() || !rawObjects.is<sol::table>() ||
        !rawInstanceId.is<std::size_t>()) {
        return nilObject(lua);
    }
    const sol::table classTable = rawClass.as<sol::table>();
    sol::table root = lua.create_table();
    bool foundRoot = false;
    for (const sol::table& candidate : nativeRoots(lua, classTable)) {
        if (objectsRawEqual(candidate, nativeType) ||
            derivesFrom(lua, candidate, nativeType)) {
            root = candidate;
            foundRoot = true;
            break;
        }
    }
    if (!foundRoot) {
        return nilObject(lua);
    }
    const sol::object rawMinimum =
        root.raw_get<sol::object>("__classFactoryMinArgs");
    if (!rawMinimum.is<lua_Integer>() || rawMinimum.as<lua_Integer>() != 0) {
        return nilObject(lua);
    }
    constructNativeRoot(lua, classTable, instance, root, nilObject(lua));
    return nativeObjectForType(lua, fields, nativeType);
}

sol::object createNativeInstance(sol::state_view lua,
                                 const sol::table& classTable,
                                 const sol::object& rawConstructorArguments,
                                 bool allowDeferredRoots) {
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return nilObject(lua);
    }
    sol::table constructorArguments = lua.create_table();
    if (rawConstructorArguments.valid() &&
        rawConstructorArguments.get_type() != sol::type::lua_nil) {
        if (!rawConstructorArguments.is<sol::table>()) {
            throw std::invalid_argument(
                "Class allocator expects a native constructor argument map");
        }
        constructorArguments = rawConstructorArguments.as<sol::table>();
        for (const auto& entry : constructorArguments) {
            if (!entry.first.is<sol::table>()) {
                throw std::invalid_argument(
                    "Native constructor map keys must be native root types");
            }
            bool knownRoot = false;
            for (const sol::table& root : roots) {
                if (objectsRawEqual(entry.first.as<sol::table>(), root)) {
                    knownRoot = true;
                    break;
                }
            }
            if (!knownRoot) {
                throw std::invalid_argument(
                    "Native constructor map contains a type that is not a "
                    "native root");
            }
            if (!entry.second.is<sol::table>()) {
                throw std::invalid_argument(
                    "Native constructor map values must be packed argument "
                    "tables");
            }
        }
    }
    sol::table fields = lua.create_table();
    sol::table nativeObjects = lua.create_table();
    fields.raw_set("__class", classTable);
    fields.raw_set("__nativeObjects", nativeObjects);
    const std::size_t instanceId = nextInstanceId(lua);
    fields.raw_set("__instanceId", instanceId);
    fields.raw_set(NATIVE_INITIALIZING_FIELD, true);
    lua_newuserdatauv(lua.lua_state(), 1, 1);
    constructingCompositeMetatable(lua).push();
    lua_setmetatable(lua.lua_state(), -2);
    fields.push();
    lua_setiuservalue(lua.lua_state(), -2, 1);
    sol::object instance = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    registryTable(lua, INSTANCES_KEY, "v").raw_set(instanceId, instance);
    try {
        for (const sol::table& root : roots) {
            const sol::object arguments =
                constructorArguments.raw_get<sol::object>(root);
            if (allowDeferredRoots && !arguments.is<sol::table>() &&
                nativeRootIsDeferred(root)) {
                continue;
            }
            constructNativeRoot(lua, classTable, instance, root, arguments);
        }
    } catch (...) {
        failNativeConstruction(lua, classTable, instance);
        throw;
    }
    return instance;
}

sol::object allocateInstance(
    sol::state_view lua, const sol::table& classTable,
    const sol::object& constructorArguments = sol::object(),
    bool allowDeferredRoots = false) {
    sol::object instance = createNativeInstance(
        lua, classTable, constructorArguments, allowDeferredRoots);
    if (!instance.valid() || instance.get_type() == sol::type::lua_nil) {
        sol::table tableInstance = lua.create_table();
        tableInstance.raw_set("__class", classTable);
        tableInstance[sol::metatable_key] = classTable;
        instance = tableInstance;
    }
    return instance;
}

sol::table constructorClass(lua_State* state) {
    return sol::stack::get<sol::table>(state, lua_upvalueindex(1));
}

struct CallableInfo {
    int parameterCount = 0;
    bool vararg = false;
    std::vector<std::string> parameterNames;
};

CallableInfo inspectCallable(const sol::object& callable) {
    CallableInfo result;
    if (callable.get_type() != sol::type::function) {
        return result;
    }
    lua_State* state = callable.lua_state();
    callable.push();
    lua_Debug info{};
    if (lua_getinfo(state, ">u", &info) == 0) {
        return result;
    }
    result.parameterCount = static_cast<int>(info.nparams);
    result.vararg = info.isvararg != 0;
    callable.push();
    result.parameterNames.reserve(info.nparams);
    for (int index = 1; index <= static_cast<int>(info.nparams); ++index) {
        const char* name = lua_getlocal(state, nullptr, index);
        if (name != nullptr && std::string(name) != "self") {
            result.parameterNames.emplace_back(name);
        }
    }
    lua_pop(state, 1);
    return result;
}

void callConstructorFunction(lua_State* state, const sol::object& function,
                             const sol::object& instance, int firstArgument,
                             int originalTop) {
    const CallableInfo info = inspectCallable(function);
    const int argumentCount =
        firstArgument <= originalTop ? originalTop - firstArgument + 1 : 0;
    const int maximumArgumentCount = std::max(0, info.parameterCount - 1);
    if (!info.vararg && argumentCount > maximumArgumentCount) {
        throw std::invalid_argument(
            "Class initializer received too many arguments");
    }
    function.push();
    instance.push();
    for (int index = firstArgument; index <= originalTop; ++index) {
        lua_pushvalue(state, index);
    }
    if (ludork::standard::protectedLuaCall(
            state, originalTop - firstArgument + 2, 0) != LUA_OK) {
        size_t length = 0;
        const char* message = luaL_tolstring(state, -1, &length);
        const std::string error = message == nullptr
                                      ? "Class initializer failed"
                                      : std::string(message, length);
        lua_pop(state, 2);
        throw std::runtime_error(error);
    }
}

int constructClassInstance(lua_State* state, int firstArgument) {
    sol::state_view lua(state);
    const int originalTop = lua_gettop(state);
    const sol::table classTable = constructorClass(state);
    const sol::object initializer =
        findScriptMember(lua, classTable, sol::make_object(lua, "init"));
    const bool hasInitializer = initializer.is<sol::function>();
    sol::object instance;
    if (!hasInitializer && firstArgument <= originalTop) {
        const std::vector<sol::table> roots = nativeRoots(lua, classTable);
        if (roots.size() > 1) {
            throw std::invalid_argument(
                "Class with multiple native roots and constructor arguments "
                "must define init");
        }
        if (roots.size() == 1) {
            sol::table arguments = lua.create_table();
            const int argumentCount = originalTop - firstArgument + 1;
            arguments.raw_set("n", argumentCount);
            for (int index = firstArgument; index <= originalTop; ++index) {
                arguments.raw_set(index - firstArgument + 1,
                                  sol::stack::get<sol::object>(state, index));
            }
            sol::table constructorArguments = lua.create_table();
            constructorArguments.raw_set(roots.front(), arguments);
            instance = allocateInstance(lua, classTable, constructorArguments);
        } else {
            throw std::invalid_argument(
                "Class without init does not accept constructor arguments");
        }
    } else {
        instance =
            allocateInstance(lua, classTable, sol::object(), hasInitializer);
    }
    try {
        validateNativeInstanceShape(lua, classTable, instance);
        syncNativeClassDefaults(lua, classTable, instance);
        if (hasInitializer) {
            callConstructorFunction(state, initializer, instance, firstArgument,
                                    originalTop);
        }
        if (hasInitializer) {
            completeDefaultNativeRoots(lua, classTable, instance);
        }
        validateNativeRoots(lua, classTable, instance);
        finishNativeConstruction(lua, classTable, instance);
    } catch (...) {
        failNativeConstruction(lua, classTable, instance);
        throw;
    }
    instance.push();
    return 1;
}

int classNew(lua_State* state) {
    try {
        return constructClassInstance(state, 1);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classCall(lua_State* state) {
    try {
        return constructClassInstance(state, 2);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classInstanceIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table classTable = constructorClass(state);
        const sol::object target = sol::stack::get<sol::object>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object getter =
            findAccessor(lua, classTable, "__getters", key);
        if (getter.is<sol::function>()) {
            getter.push();
            target.push();
            lua_call(state, 1, 1);
            return 1;
        }
        findInClass(lua, classTable, key).push();
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classInstanceNewIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table classTable = constructorClass(state);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object setter =
            findAccessor(lua, classTable, "__setters", key);
        if (setter.is<sol::function>()) {
            setter.push();
            lua_pushvalue(state, 1);
            lua_pushvalue(state, 3);
            lua_call(state, 2, 0);
            return 0;
        }
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classMetatableIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        findInClass(lua, constructorClass(state), key, false).push();
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classMetatableNewIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        sol::table classTable = constructorClass(state);
        const sol::object value = sol::stack::get<sol::object>(state, 3);
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        invalidateClassLookup(lua, classTable);
        if (value.is<sol::function>()) {
            const sol::object implementationOwner =
                classTable.raw_get<sol::object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != sol::type::lua_nil) {
                classTable.raw_set("_hasImplementationOwner", sol::lua_nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

void setClassClosure(lua_State* state, const sol::table& target,
                     const char* name, const sol::table& classTable,
                     lua_CFunction function) {
    target.push();
    lua_pushstring(state, name);
    classTable.push();
    lua_pushcclosure(state, function, 1);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

bool isFinalizedClass(const sol::table& value) {
    return isClass(value) && tableHasMetatable(value) &&
           value.raw_get<sol::object>("__bases").is<sol::table>() &&
           value.raw_get<sol::object>("__mro").is<sol::table>() &&
           value.raw_get<sol::object>("__index").is<sol::function>() &&
           value.raw_get<sol::object>("__newindex").is<sol::function>() &&
           value.raw_get<sol::object>("new").is<sol::function>();
}

void validateClassDefinition(const sol::table& definition) {
    if (rawBool(definition, "__ludorkClass")) {
        throw std::invalid_argument("Class definition is already finalized");
    }
    if (tableHasMetatable(definition)) {
        throw std::invalid_argument(
            "Class definition must be a plain table without a metatable");
    }
    for (const char* name : CLASS_RESERVED_FIELDS) {
        const sol::object value = definition.raw_get<sol::object>(name);
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "Class definition contains reserved field '" +
                std::string(name) + "'");
        }
    }
}

sol::table normalizeClassBases(sol::state_view lua, const sol::table& bases) {
    sol::table result = lua.create_table();
    std::vector<sol::table> accepted;
    accepted.reserve(bases.size());
    for (std::size_t index = 1; index <= bases.size(); ++index) {
        const sol::object rawBase = bases.raw_get<sol::object>(index);
        if (!rawBase.is<sol::table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        const sol::table base = rawBase.as<sol::table>();
        if (!isFinalizedClass(base) && !isNativeType(lua, base)) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        for (const sol::table& previous : accepted) {
            if (objectsRawEqual(previous, base)) {
                throw std::invalid_argument(
                    "Class bases must not contain duplicates");
            }
        }
        accepted.push_back(base);
        result.add(base);
    }
    return result;
}

sol::table finalizeClassImpl(sol::table definition, const sol::table& bases) {
    sol::state_view lua(definition.lua_state());
    validateClassDefinition(definition);
    const sol::table baseList = normalizeClassBases(lua, bases);
    const std::vector<sol::table> linearization =
        createMro(definition, baseList, MroKind::Runtime);
    for (std::size_t index = 1; index < linearization.size(); ++index) {
        if (isNativeType(lua, linearization[index])) {
            ensureNativeInitializer(lua, linearization[index]);
        }
    }
    std::vector<sol::object> ownMethods;
    for (const auto& entry : definition) {
        if (entry.second.is<sol::function>()) {
            ownMethods.push_back(entry.second);
        }
    }
    sol::table mro = lua.create_table();
    for (const sol::table& type : linearization) {
        mro.add(type);
    }

    sol::table classTable = definition;
    classTable.raw_set("__ludorkClass", true);
    classTable.raw_set("__lookupVersion", 1);
    classTable.raw_set("__bases", baseList);
    if (baseList.size() > 0) {
        classTable.raw_set("__base", baseList[1]);
    }
    classTable.raw_set("__mro", mro);
    ensureMroSet(lua, classTable, mro, "__mroSet");
    for (const sol::table& base : tableList(baseList)) {
        registerSubclass(lua, base, classTable);
    }
    setClassClosure(lua.lua_state(), classTable, "__index", classTable,
                    classInstanceIndex);
    setClassClosure(lua.lua_state(), classTable, "__newindex", classTable,
                    classInstanceNewIndex);
    setClassClosure(lua.lua_state(), classTable, "__gc", classTable,
                    classInstanceGc);
    setClassClosure(lua.lua_state(), classTable, "new", classTable, classNew);
    sol::table classMetatable = lua.create_table();
    setClassClosure(lua.lua_state(), classMetatable, "__index", classTable,
                    classMetatableIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__newindex", classTable,
                    classMetatableNewIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__call", classTable,
                    classCall);
    classTable[sol::metatable_key] = classMetatable;
    for (const sol::object& method : ownMethods) {
        registerMethodOwner(lua, classTable, method);
    }
    return classTable;
}

sol::table classFunction(sol::this_state state, const sol::object& definition,
                         sol::variadic_args bases) {
    sol::state_view lua(state);
    if (!definition.is<sol::table>()) {
        throw std::invalid_argument("Class definition must be a table");
    }
    sol::table baseList = lua.create_table();
    for (const sol::stack_proxy& rawBase : bases) {
        const sol::object base = sol::make_object(lua, rawBase);
        if (!base.is<sol::table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        baseList.add(base);
    }
    return finalizeClassImpl(definition.as<sol::table>(), baseList);
}

sol::table ownFields(sol::state_view lua, const sol::object& target) {
    if (target.is<sol::table>()) {
        return target.as<sol::table>();
    }
    if (target.get_type() == sol::type::userdata) {
        return getUserFields(lua, target, false);
    }
    return lua.create_table();
}

sol::object rawOwnField(sol::state_view lua, const sol::object& target,
                        const sol::object& key) {
    if (target.get_type() == sol::type::userdata) {
        lua_State* state = lua.lua_state();
        target.push();
        if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
            lua_pop(state, 2);
            return nilObject(lua);
        }
        key.push();
        lua_rawget(state, -2);
        sol::object result = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 3);
        return result;
    }
    if (target.is<sol::table>()) {
        return target.as<sol::table>().raw_get<sol::object>(key);
    }
    return nilObject(lua);
}

bool hasRawOwnField(sol::state_view lua, const sol::object& target,
                    const sol::object& key) {
    const sol::object value = rawOwnField(lua, target, key);
    return value.valid() && value.get_type() != sol::type::lua_nil;
}

sol::table ownKeyList(sol::state_view lua, const sol::object& target) {
    sol::table result = lua.create_table();
    for (const auto& entry : ownFields(lua, target)) {
        result.add(entry.first);
    }
    return result;
}

sol::table mroCopy(sol::state_view lua, const sol::object& value) {
    sol::object rawClass = value;
    if (!value.is<sol::table>() ||
        (!isClass(value.as<sol::table>()) &&
         !isNativeType(lua, value.as<sol::table>()))) {
        rawClass = actualClassOf(lua, value);
    }
    if (!rawClass.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.getMro requires a class or class instance");
    }
    const sol::table mro = getMro(lua, rawClass.as<sol::table>());
    sol::table result = lua.create_table();
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        result.add(mro.raw_get<sol::object>(index));
    }
    return result;
}

sol::table getParameterNames(const sol::object& callable) {
    sol::state_view lua(callable.lua_state());
    if (callable.get_type() != sol::type::function) {
        throw std::invalid_argument(
            "Class.getParameterNames requires a function");
    }
    const CallableInfo info = inspectCallable(callable);
    sol::table result = lua.create_table();
    for (const std::string& name : info.parameterNames) {
        result.add(name);
    }
    return result;
}

sol::object constructNamed(sol::this_state state, const sol::object& rawType,
                           const sol::object& rawArguments) {
    sol::state_view lua(state);
    if (!rawType.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.constructNamed requires a class type");
    }
    sol::table arguments = lua.create_table();
    if (rawArguments.valid() && rawArguments.get_type() != sol::type::lua_nil) {
        if (!rawArguments.is<sol::table>()) {
            throw std::invalid_argument(
                "Class.constructNamed arguments must be a table");
        }
        arguments = rawArguments.as<sol::table>();
    }
    const sol::table type = rawType.as<sol::table>();
    sol::object initializer = nilObject(lua);
    if (isClass(type)) {
        initializer =
            findScriptMember(lua, type, sol::make_object(lua, "init"));
    } else {
        initializer = rawMember(lua, type, sol::make_object(lua, "init"));
    }
    std::vector<sol::object> values;
    if (initializer.get_type() == sol::type::function) {
        const CallableInfo info = inspectCallable(initializer);
        if (info.parameterNames.empty() && !tableIsEmpty(arguments)) {
            throw std::invalid_argument(
                "Class initializer exposes no named parameters");
        }
        values.reserve(info.parameterNames.size());
        for (const std::string& name : info.parameterNames) {
            const sol::object value =
                protectedIndex(lua, sol::make_object(lua, arguments),
                               sol::make_object(lua, name));
            values.push_back(value.valid() ? value : nilObject(lua));
        }
    } else if (!tableIsEmpty(arguments)) {
        throw std::invalid_argument(
            "Class without init does not accept named arguments");
    }
    const sol::object rawConstructor =
        protectedIndex(lua, rawType, sol::make_object(lua, "new"));
    if (!rawConstructor.is<sol::protected_function>()) {
        throw std::runtime_error("Class type has no new constructor");
    }
    sol::protected_function constructor =
        rawConstructor.as<sol::protected_function>();
    sol::protected_function_result result = constructor(sol::as_args(values));
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    return result.get<sol::object>();
}

bool isSubclass(sol::this_state state, const sol::table& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(sol::state_view(state),
                                                         value, targetClass);
}

bool isInstance(sol::this_state state, const sol::object& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isInstanceOf(sol::state_view(state),
                                                         value, targetClass);
}

sol::object classType(sol::this_state state, const sol::object& value) {
    return ludork::standard::class_runtime::typeOf(sol::state_view(state),
                                                   value);
}

sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result) {
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    return result.return_count() == 0 ? nilObject(lua)
                                      : result.get<sol::object>();
}

sol::table runtimeResolverResult(sol::state_view lua,
                                 const std::vector<sol::object>& values) {
    sol::table result = lua.create_table(static_cast<int>(values.size()), 1);
    result.raw_set("n", values.size());
    std::size_t index = 1;
    for (const sol::object& value : values) {
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            result.raw_set(index, value);
        }
        ++index;
    }
    return result;
}

sol::object runtimeResolverArgument(sol::state_view lua,
                                    const sol::table& arguments,
                                    std::size_t index) {
    const sol::object value = arguments.raw_get<sol::object>(index);
    return value.valid() ? value : nilObject(lua);
}

void registerRuntimeService(sol::this_state state, const std::string& name,
                            const sol::protected_function& callback) {
    ludork::standard::class_runtime::registerService(sol::state_view(state),
                                                     name, callback);
}

void unregisterRuntimeService(sol::this_state state, const std::string& name) {
    ludork::standard::class_runtime::unregisterService(sol::state_view(state),
                                                       name);
}

sol::table callRuntimeService(sol::state_view lua, const std::string& operation,
                              const sol::table& arguments) {
    const sol::object rawService = registryTable(lua, RUNTIME_SERVICES_KEY)
                                       .raw_get<sol::object>(operation);
    if (!rawService.is<sol::protected_function>()) {
        return runtimeResolverResult(lua, {});
    }
    std::size_t count = arguments.size();
    const sol::object rawCount = arguments.raw_get<sol::object>("n");
    if (rawCount.is<std::size_t>()) {
        count = rawCount.as<std::size_t>();
    }
    std::vector<sol::object> values;
    values.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        values.push_back(runtimeResolverArgument(lua, arguments, index));
    }
    sol::protected_function service = rawService.as<sol::protected_function>();
    sol::protected_function_result called = service(sol::as_args(values));
    if (!called.valid()) {
        const sol::error error = called;
        throw std::runtime_error(error.what());
    }
    std::vector<sol::object> results;
    results.reserve(called.return_count());
    for (int index = 0; index < called.return_count(); ++index) {
        results.push_back(called.get<sol::object>(index));
    }
    return runtimeResolverResult(lua, results);
}

std::vector<sol::table> runtimeClassMro(sol::state_view lua,
                                        const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object value = mro.raw_get<sol::object>(index);
        if (value.is<sol::table>()) {
            result.push_back(value.as<sol::table>());
        }
    }
    if (result.empty()) {
        result.push_back(classTable);
    }
    return result;
}

std::string runtimeValueKind(const sol::object& value) {
    switch (value.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
            return "nil";
        case sol::type::boolean:
            return "boolean";
        case sol::type::number:
            return "number";
        case sol::type::string:
            return "string";
        case sol::type::table:
            return "table";
        case sol::type::function:
            return "function";
        case sol::type::userdata:
        case sol::type::lightuserdata:
            return "userdata";
        case sol::type::thread:
            return "thread";
        default:
            return "nil";
    }
}

sol::object runtimeIndex(sol::state_view lua, const sol::object& target,
                         const sol::object& key, bool raw) {
    if (raw && target.get_type() == sol::type::userdata) {
        return getUserFields(lua, target, false).raw_get<sol::object>(key);
    }
    if (raw && !target.is<sol::table>()) {
        return nilObject(lua);
    }
    if (!raw) {
        return protectedIndex(lua, target, key);
    }
    lua_State* state = lua.lua_state();
    target.push();
    key.push();
    lua_rawget(state, -2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 2);
    return result;
}

void runtimeAssign(sol::state_view lua, const sol::object& target,
                   const sol::object& key, const sol::object& value, bool raw) {
    if (raw && !target.is<sol::table>()) {
        throw std::invalid_argument("Raw assignment requires a table");
    }
    if (!raw) {
        protectedAssign(lua, target, key, value);
        return;
    }
    lua_State* state = lua.lua_state();
    target.push();
    key.push();
    value.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

std::vector<sol::object> runtimeKeys(sol::state_view lua,
                                     const sol::object& target, bool raw) {
    std::vector<sol::object> keys;
    if (raw) {
        sol::table source;
        if (target.is<sol::table>()) {
            source = target.as<sol::table>();
        } else if (target.get_type() == sol::type::userdata) {
            source = getUserFields(lua, target, false);
        } else {
            return keys;
        }
        for (const auto& entry : source) {
            keys.push_back(entry.first);
        }
        return keys;
    }

    const sol::object rawPairs = lua.globals().raw_get<sol::object>("pairs");
    if (!rawPairs.is<sol::protected_function>()) {
        throw std::runtime_error("Lua pairs function is not defined");
    }
    sol::protected_function pairs = rawPairs.as<sol::protected_function>();
    sol::protected_function_result initialized = pairs(target);
    if (!initialized.valid()) {
        const sol::error error = initialized;
        throw std::runtime_error(error.what());
    }
    if (initialized.return_count() < 3) {
        return keys;
    }
    const sol::object rawIterator = initialized.get<sol::object>(0);
    if (!rawIterator.is<sol::protected_function>()) {
        return keys;
    }
    sol::protected_function iterator =
        rawIterator.as<sol::protected_function>();
    const sol::object iteratorState = initialized.get<sol::object>(1);
    sol::object control = initialized.get<sol::object>(2);
    for (;;) {
        sol::protected_function_result next = iterator(iteratorState, control);
        if (!next.valid()) {
            const sol::error error = next;
            throw std::runtime_error(error.what());
        }
        if (next.return_count() == 0) {
            break;
        }
        sol::object key = next.get<sol::object>(0);
        if (!key.valid() || key.get_type() == sol::type::lua_nil) {
            break;
        }
        keys.push_back(key);
        control = key;
    }
    return keys;
}

void copyTableMetatable(const sol::table& source, const sol::table& target) {
    lua_State* state = source.lua_state();
    source.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return;
    }
    target.push();
    lua_pushvalue(state, -2);
    lua_setmetatable(state, -2);
    lua_pop(state, 3);
}

bool isAtomicClassTable(sol::state_view lua, const sol::table& value) {
    return isClass(value) || isNativeType(lua, value);
}

sol::object copyNativeValue(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return value;
    }
    const sol::object rawType = nativeTypeOf(lua, value);
    if (!rawType.is<sol::table>()) {
        return value;
    }
    const sol::object rawCopy = protectedIndex(
        lua, rawType, sol::make_object(lua, std::string(NATIVE_COPY_FIELD)));
    if (!rawCopy.is<sol::protected_function>()) {
        return value;
    }
    sol::protected_function copy = rawCopy.as<sol::protected_function>();
    sol::protected_function_result result = copy(value);
    return checkedResult(lua, result);
}

sol::object shallowCopyImpl(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::table) {
        return copyNativeValue(lua, value);
    }
    const sol::table source = value.as<sol::table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    sol::table result = lua.create_table();
    for (const auto& entry : source) {
        result.raw_set(entry.first, entry.second);
    }
    copyTableMetatable(source, result);
    return sol::make_object(lua, result);
}

sol::object deepCopyImpl(sol::state_view lua, const sol::object& value,
                         std::unordered_map<const void*, sol::object>& visited);

struct NativeDeepCopyContext {
    sol::state_view lua;
    std::unordered_map<const void*, sol::object>* visited = nullptr;
};

sol::object deepCopyNativeChild(void* rawContext, const sol::object& value) {
    auto* context = static_cast<NativeDeepCopyContext*>(rawContext);
    if (context == nullptr || context->visited == nullptr) {
        throw std::runtime_error("Native deep-copy context is unavailable");
    }
    return deepCopyImpl(context->lua, value, *context->visited);
}

sol::object deepCopyNativeValue(
    sol::state_view lua, const sol::object& value, const void* identity,
    std::unordered_map<const void*, sol::object>& visited) {
    const sol::object rawType = nativeTypeOf(lua, value);
    if (rawType.get_type() != sol::type::table) {
        visited.emplace(identity, value);
        return value;
    }
    const auto protocol = findNativeDeepCopyProtocol(lua, rawType);
    if (!protocol.has_value()) {
        const sol::object result = copyNativeValue(lua, value);
        visited.emplace(identity, result);
        return result;
    }
    NativeDeepCopyContext context{lua, &visited};
    if (protocol->mode ==
        ludork::standard::class_runtime::NativeDeepCopyMode::TwoPhase) {
        if (protocol->create == nullptr || protocol->populate == nullptr) {
            throw std::runtime_error(
                "Native two-phase deep-copy protocol is incomplete");
        }
        const sol::object result = protocol->create(lua, value);
        visited.emplace(identity, result);
        protocol->populate(lua, value, result, &deepCopyNativeChild, &context);
        return result;
    }
    if (protocol->build == nullptr) {
        throw std::runtime_error(
            "Native deferred deep-copy protocol is incomplete");
    }
    const sol::object result =
        protocol->build(lua, value, &deepCopyNativeChild, &context);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return existing->second;
    }
    visited.emplace(identity, result);
    return result;
}

sol::object deepCopyImpl(
    sol::state_view lua, const sol::object& value,
    std::unordered_map<const void*, sol::object>& visited) {
    if (value.get_type() != sol::type::table) {
        if (value.get_type() != sol::type::userdata) {
            return value;
        }
        lua_State* state = lua.lua_state();
        value.push();
        const void* identity = lua_topointer(state, -1);
        lua_pop(state, 1);
        const auto existing = visited.find(identity);
        if (existing != visited.end()) {
            return existing->second;
        }
        return deepCopyNativeValue(lua, value, identity, visited);
    }
    const sol::table source = value.as<sol::table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    lua_State* state = lua.lua_state();
    source.push();
    const void* identity = lua_topointer(state, -1);
    lua_pop(state, 1);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return existing->second;
    }
    sol::table result = lua.create_table();
    visited.emplace(identity, sol::make_object(lua, result));
    for (const auto& entry : source) {
        const sol::object key = deepCopyImpl(lua, entry.first, visited);
        const sol::object item = deepCopyImpl(lua, entry.second, visited);
        result.raw_set(key, item);
    }
    copyTableMetatable(source, result);
    return sol::make_object(lua, result);
}

sol::object deepCopyImpl(sol::state_view lua, const sol::object& value) {
    std::unordered_map<const void*, sol::object> visited;
    return deepCopyImpl(lua, value, visited);
}

sol::object clonePlainDataImpl(
    sol::state_view lua, const sol::object& value,
    std::unordered_map<const void*, sol::table>& visited) {
    if (value.get_type() != sol::type::table) {
        return value;
    }
    lua_State* state = lua.lua_state();
    value.push();
    if (lua_getmetatable(state, -1) != 0) {
        lua_pop(state, 2);
        return value;
    }
    const void* identity = lua_topointer(state, -1);
    lua_pop(state, 1);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return sol::make_object(lua, existing->second);
    }

    const sol::table source = value.as<sol::table>();
    sol::table result = lua.create_table();
    visited.emplace(identity, result);
    for (const auto& entry : source) {
        const sol::object key = clonePlainDataImpl(lua, entry.first, visited);
        const sol::object item = clonePlainDataImpl(lua, entry.second, visited);
        result.raw_set(key, item);
    }

    return sol::make_object(lua, result);
}

sol::object clonePlainDataImpl(sol::state_view lua, const sol::object& value) {
    std::unordered_map<const void*, sol::table> visited;
    return clonePlainDataImpl(lua, value, visited);
}

sol::table runtimeStringKeys(sol::state_view lua,
                             const std::vector<sol::object>& keys) {
    sol::table result = lua.create_table();
    std::size_t index = 1;
    for (const sol::object& key : keys) {
        if (key.is<std::string>()) {
            result.raw_set(index++, key);
        }
    }
    return result;
}

sol::table runtimeMro(sol::state_view lua, const sol::object& value) {
    sol::table result = lua.create_table();
    if (!value.is<sol::table>()) {
        return result;
    }
    const std::vector<sol::table> mro =
        runtimeClassMro(lua, value.as<sol::table>());
    std::size_t index = 1;
    for (const sol::table& type : mro) {
        result.raw_set(index++, type);
    }
    return result;
}

sol::table callRuntimeMethod(sol::state_view lua, const sol::object& receiver,
                             const sol::object& rawName,
                             const sol::object& rawArguments) {
    if (!rawName.is<std::string>()) {
        throw std::invalid_argument("Runtime method name must be a string");
    }
    const std::string name = rawName.as<std::string>();
    const sol::object rawMethod =
        runtimeIndex(lua, receiver, sol::make_object(lua, name), false);
    if (!rawMethod.is<sol::protected_function>()) {
        throw std::runtime_error("Runtime method is not defined: " + name);
    }
    std::vector<sol::object> arguments;
    if (rawArguments.is<sol::table>()) {
        const sol::table table = rawArguments.as<sol::table>();
        std::size_t count = table.size();
        const sol::object rawCount = table.raw_get<sol::object>("n");
        if (rawCount.is<std::size_t>()) {
            count = rawCount.as<std::size_t>();
        }
        arguments.reserve(count + 1);
        arguments.push_back(receiver);
        for (std::size_t index = 1; index <= count; ++index) {
            arguments.push_back(runtimeResolverArgument(lua, table, index));
        }
    } else {
        arguments.push_back(receiver);
    }
    sol::protected_function method = rawMethod.as<sol::protected_function>();
    sol::protected_function_result called = method(sol::as_args(arguments));
    if (!called.valid()) {
        const sol::error error = called;
        throw std::runtime_error(error.what());
    }
    std::vector<sol::object> results;
    results.reserve(called.return_count());
    for (int index = 0; index < called.return_count(); ++index) {
        results.push_back(called.get<sol::object>(index));
    }
    return runtimeResolverResult(lua, results);
}

sol::object constructRuntimeClass(sol::state_view lua,
                                  const sol::object& rawClass,
                                  const sol::table& arguments) {
    if (!rawClass.is<sol::table>()) {
        throw std::invalid_argument(
            "Runtime class constructor requires a class");
    }
    const sol::object rawConstructor =
        rawClass.as<sol::table>().get<sol::object>("new");
    if (!rawConstructor.is<sol::protected_function>()) {
        throw std::runtime_error("Runtime class has no new constructor");
    }
    std::size_t count = arguments.size();
    const sol::object rawCount = arguments.raw_get<sol::object>("n");
    if (rawCount.is<std::size_t>()) {
        count = rawCount.as<std::size_t>();
    }
    std::vector<sol::object> values;
    if (count > 1) {
        values.reserve(count - 1);
    }
    for (std::size_t index = 2; index <= count; ++index) {
        values.push_back(runtimeResolverArgument(lua, arguments, index));
    }
    sol::protected_function constructor =
        rawConstructor.as<sol::protected_function>();
    sol::protected_function_result result = constructor(sol::as_args(values));
    return checkedResult(lua, result);
}

sol::table invokeRuntimeCallable(sol::state_view lua,
                                 const sol::object& rawCallable,
                                 const sol::object& rawArguments) {
    if (!rawCallable.is<sol::protected_function>()) {
        throw std::invalid_argument("Runtime callable must be a function");
    }
    std::vector<sol::object> arguments;
    if (rawArguments.is<sol::table>()) {
        const sol::table packed = rawArguments.as<sol::table>();
        std::size_t count = packed.size();
        const sol::object rawCount = packed.raw_get<sol::object>("n");
        if (rawCount.is<std::size_t>()) {
            count = rawCount.as<std::size_t>();
        }
        arguments.reserve(count);
        for (std::size_t index = 1; index <= count; ++index) {
            arguments.push_back(runtimeResolverArgument(lua, packed, index));
        }
    }
    sol::protected_function callable =
        rawCallable.as<sol::protected_function>();
    sol::protected_function_result called = callable(sol::as_args(arguments));
    if (!called.valid()) {
        const sol::error error = called;
        throw std::runtime_error(error.what());
    }
    std::vector<sol::object> results;
    results.reserve(called.return_count());
    for (int index = 0; index < called.return_count(); ++index) {
        results.push_back(called.get<sol::object>(index));
    }
    return runtimeResolverResult(lua, results);
}

sol::table runtimeClassResolver(sol::this_state state,
                                const std::string& operation,
                                const sol::table& arguments) {
    sol::state_view lua(state);
    const sol::object first = runtimeResolverArgument(lua, arguments, 1);
    const sol::object second = runtimeResolverArgument(lua, arguments, 2);
    if (operation == "reflect.type") {
        return runtimeResolverResult(lua, {classType(state, first)});
    }
    if (operation == "reflect.isSubclass") {
        const bool result =
            first.is<sol::table>() && second.is<sol::table>() &&
            isSubclass(state, first.as<sol::table>(), second.as<sol::table>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "reflect.isInstance") {
        const bool result = second.is<sol::table>() &&
                            isInstance(state, first, second.as<sol::table>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "reflect.equal") {
        return runtimeResolverResult(
            lua, {sol::make_object(lua, objectsRawEqual(first, second))});
    }
    if (operation == "reflect.mro") {
        return runtimeResolverResult(
            lua, {sol::make_object(lua, runtimeMro(lua, first))});
    }
    if (operation == "reflect.keys" || operation == "reflect.rawKeys") {
        const std::vector<sol::object> keys =
            runtimeKeys(lua, first, operation == "reflect.rawKeys");
        return runtimeResolverResult(
            lua, {sol::make_object(lua, runtimeStringKeys(lua, keys))});
    }
    if (operation == "reflect.get" || operation == "reflect.rawGet") {
        return runtimeResolverResult(
            lua,
            {runtimeIndex(lua, first, second, operation == "reflect.rawGet")});
    }
    if (operation == "reflect.set") {
        const sol::object value = runtimeResolverArgument(lua, arguments, 3);
        runtimeAssign(lua, first, second, value, false);
        return runtimeResolverResult(lua, {});
    }
    if (operation == "reflect.kind") {
        return runtimeResolverResult(
            lua, {sol::make_object(lua, runtimeValueKind(first))});
    }
    if (operation == "reflect.tostring") {
        const sol::object rawToString =
            lua.globals().raw_get<sol::object>("tostring");
        if (!rawToString.is<sol::protected_function>()) {
            throw std::runtime_error("Lua tostring function is not defined");
        }
        sol::protected_function toString =
            rawToString.as<sol::protected_function>();
        sol::protected_function_result result = toString(first);
        return runtimeResolverResult(lua, {checkedResult(lua, result)});
    }
    if (operation == "reflect.construct" || operation == "class.construct") {
        return runtimeResolverResult(
            lua, {constructRuntimeClass(lua, first, arguments)});
    }
    if (operation == "reflect.call") {
        return callRuntimeMethod(lua, first, second,
                                 runtimeResolverArgument(lua, arguments, 3));
    }
    if (operation == "reflect.invoke") {
        return invokeRuntimeCallable(lua, first, second);
    }
    if (operation == "reflect.clone") {
        return runtimeResolverResult(lua, {clonePlainDataImpl(lua, first)});
    }
    return callRuntimeService(lua, operation, arguments);
}

bool hasOwnFieldFunction(sol::this_state state, const sol::object& target,
                         const sol::object& key) {
    return hasRawOwnField(sol::state_view(state), target, key);
}

sol::table getMroFunction(sol::this_state state, const sol::object& value) {
    return mroCopy(sol::state_view(state), value);
}

sol::object copyFunction(sol::this_state state, const sol::object& value) {
    return shallowCopyImpl(sol::state_view(state), value);
}

sol::object deepCopyFunction(sol::this_state state, const sol::object& value) {
    return deepCopyImpl(sol::state_view(state), value);
}

}  // namespace

namespace ludork::standard::class_runtime {

sol::table finalizeClass(sol::table definition, const sol::table& bases) {
    return finalizeClassImpl(std::move(definition), bases);
}

void registerNativeClass(sol::table nativeType, const sol::table& metadata) {
    sol::state_view lua(nativeType.lua_state());
    sol::table defaults = lua.create_table();
    const sol::object rawAttrs = metadata.raw_get<sol::object>("attrs");
    if (rawAttrs.is<sol::table>()) {
        const sol::table attrs = rawAttrs.as<sol::table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const sol::object rawName = attrs.raw_get<sol::object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const sol::object rawField =
                metadata.raw_get<sol::object>(rawName.as<std::string>());
            if (!rawField.is<sol::table>()) {
                continue;
            }
            const sol::object rawDefault =
                rawField.as<sol::table>().raw_get<sol::object>("default");
            if (!rawDefault.valid() ||
                rawDefault.get_type() == sol::type::lua_nil) {
                continue;
            }
            const sol::object value = clonePlainDataImpl(lua, rawDefault);
            defaults.raw_set(rawName, value);
        }
    }
    nativeType.raw_set("__classDefaults", defaults);
    nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                       lua.create_table());

    sol::table metatable =
        getObjectMetatable(lua, sol::make_object(lua, nativeType));
    const sol::object rawGuard =
        metatable.raw_get<sol::object>(NATIVE_CLASS_GUARD_FIELD);
    if (rawGuard.is<bool>() && rawGuard.as<bool>()) {
        return;
    }
    metatable.raw_set(NATIVE_CLASS_INDEX_FIELD,
                      metatable.raw_get<sol::object>("__index"));
    metatable.raw_set(NATIVE_CLASS_NEW_INDEX_FIELD,
                      metatable.raw_get<sol::object>("__newindex"));
    metatable.set_function("__index", &nativeClassIndex);
    metatable.set_function("__newindex", &nativeClassNewIndex);
    metatable.raw_set(NATIVE_CLASS_GUARD_FIELD, true);
}

void registerNativeClassDefaultResolver(
    sol::state_view lua, const sol::protected_function& callback) {
    lua.registry().raw_set(nativeClassDefaultResolverKey(lua), callback);
}

void unregisterNativeClassDefaultResolver(sol::state_view lua) {
    lua.registry().raw_set(nativeClassDefaultResolverKey(lua), sol::lua_nil);
}

sol::object protectedGet(sol::state_view lua, const sol::object& target,
                         const sol::object& key) {
    return protectedIndex(lua, target, key);
}

void protectedSet(sol::state_view lua, const sol::object& target,
                  const sol::object& key, const sol::object& value) {
    protectedAssign(lua, target, key, value);
}

sol::object rawGetOwnField(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    return rawOwnField(lua, target, key);
}

bool hasOwnField(sol::state_view lua, const sol::object& target,
                 const sol::object& key) {
    return hasRawOwnField(lua, target, key);
}

sol::table getOwnKeys(sol::state_view lua, const sol::object& target) {
    return ownKeyList(lua, target);
}

bool rawEqual(const sol::object& left, const sol::object& right) {
    return objectsRawEqual(left, right);
}

sol::table getMroCopy(sol::state_view lua, const sol::object& value) {
    return mroCopy(lua, value);
}

sol::object typeOf(sol::state_view lua, const sol::object& value) {
    if (value.is<sol::table>() && isClass(value.as<sol::table>())) {
        return lua.globals().raw_get<sol::object>("Class");
    }
    const sol::object result = actualClassOf(lua, value);
    if (result.is<sol::table>()) {
        return result;
    }
    return sol::make_object(lua,
                            sol::type_name(lua.lua_state(), value.get_type()));
}

bool isInstanceOf(sol::state_view lua, const sol::object& value,
                  const sol::table& targetClass) {
    const sol::object rawClass = scriptClassOf(lua, value);
    if (rawClass.is<sol::table>()) {
        return derivesFrom(lua, rawClass.as<sol::table>(), targetClass);
    }
    return value.get_type() == sol::type::userdata &&
           nativeTypeAccepts(lua, targetClass, value);
}

bool isSubclassOf(sol::state_view lua, const sol::table& value,
                  const sol::table& targetClass) {
    return derivesFrom(lua, value, targetClass);
}

sol::object clonePlainData(sol::state_view lua, const sol::object& value) {
    return clonePlainDataImpl(lua, value);
}

sol::object shallowCopy(sol::state_view lua, const sol::object& value) {
    return shallowCopyImpl(lua, value);
}

sol::object deepCopy(sol::state_view lua, const sol::object& value) {
    return deepCopyImpl(lua, value);
}

sol::object requireModule(sol::state_view lua, const std::string& moduleName) {
    const sol::object rawRequire =
        lua.globals().raw_get<sol::object>("require");
    if (!rawRequire.is<sol::protected_function>()) {
        throw std::runtime_error("Lua require function is not defined");
    }
    sol::protected_function require = rawRequire.as<sol::protected_function>();
    sol::protected_function_result result = require(moduleName);
    return checkedResult(lua, result);
}

sol::table invoke(sol::state_view lua, const sol::object& callable,
                  const sol::table& arguments) {
    return invokeRuntimeCallable(lua, callable,
                                 sol::make_object(lua, arguments));
}

void registerService(sol::state_view lua, const std::string& name,
                     const sol::protected_function& callback) {
    if (name.empty()) {
        throw std::invalid_argument("Runtime service name must not be empty");
    }
    registryTable(lua, RUNTIME_SERVICES_KEY).raw_set(name, callback);
}

void unregisterService(sol::state_view lua, const std::string& name) {
    registryTable(lua, RUNTIME_SERVICES_KEY).raw_set(name, sol::lua_nil);
}

sol::table callService(sol::state_view lua, const std::string& name,
                       const sol::table& arguments) {
    return callRuntimeService(lua, name, arguments);
}

void registerNativeDeepCopyProtocol(sol::state_view lua,
                                    const sol::table& nativeType,
                                    const NativeDeepCopyProtocol& protocol) {
    if (protocol.mode == NativeDeepCopyMode::TwoPhase) {
        if (protocol.create == nullptr || protocol.populate == nullptr ||
            protocol.build != nullptr) {
            throw std::invalid_argument(
                "Invalid native two-phase deep-copy protocol");
        }
    } else if (protocol.build == nullptr || protocol.create != nullptr ||
               protocol.populate != nullptr) {
        throw std::invalid_argument(
            "Invalid native deferred deep-copy protocol");
    }
    lua_State* state = lua.lua_state();
    sol::table protocols = nativeDeepCopyProtocols(lua);
    protocols.push();
    nativeType.push();
    void* storage = lua_newuserdatauv(state, sizeof(NativeDeepCopyProtocol), 0);
    new (storage) NativeDeepCopyProtocol(protocol);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

sol::table createModule(sol::state_view lua) {
    lua.registry().raw_set(SHUTTING_DOWN_KEY, sol::lua_nil);
    sol::table root = lua.create_table();
    root.set_function("isInstance", &isInstance);
    root.set_function("isSubclass", &isSubclass);
    root.set_function("type", &classType);
    root.set_function("hasOwnField", &hasOwnFieldFunction);
    root.set_function("getMro", &getMroFunction);
    root.set_function("getParameterNames", &getParameterNames);
    root.set_function("constructNamed", &constructNamed);
    root.set_function("super", superFunction);
    root.set_function("monitor", &registerMonitor);
    root.set_function("unmonitor", &unregisterMonitor);
    root.set_function("registerService", &registerRuntimeService);
    root.set_function("unregisterService", &unregisterRuntimeService);
    root.raw_set("MISSING", lua.create_table());
    lua.globals().set_function("_LUDORK_RUNTIME_RESOLVER",
                               &runtimeClassResolver);
    lua.globals().set_function("class", &classFunction);
    lua.globals().set_function("copy", &copyFunction);
    lua.globals().set_function("deepcopy", &deepCopyFunction);
    lua["super"] = root["super"];
    return root;
}

void shutdown(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    const int stackTop = lua_gettop(state);
    lua_pushboolean(state, 1);
    lua_setfield(state, LUA_REGISTRYINDEX, SHUTTING_DOWN_KEY);
    constexpr const char* registryKeys[] = {
        METHOD_OWNERS_KEY,
        NATIVE_TYPE_CACHE_KEY,
        INSTANCES_KEY,
        COMPOSITE_METATABLE_KEY,
        CONSTRUCTING_COMPOSITE_METATABLE_KEY,
        NATIVE_OWNERS_KEY,
        NATIVE_POINTER_OWNERS_KEY,
        DYNAMIC_NATIVE_WRITERS_KEY,
        SUPER_PROXY_CACHE_KEY,
        SUPER_PROXY_METATABLE_KEY,
        MONITOR_STATES_KEY,
        RUNTIME_SERVICES_KEY,
        LIFECYCLE_STATES_KEY,
        DISPOSED_METATABLE_KEY,
        "LuaSF.JsonNullSentinel",
        "LuaSF.JsonArrayMetatable",
        "LuaSF.JsonEmptyArrayMetatable",
    };
    for (const char* key : registryKeys) {
        lua_pushnil(state);
        lua_setfield(state, LUA_REGISTRYINDEX, key);
    }
    lua_pushlightuserdata(
        state, static_cast<void*>(&nativeDeepCopyProtocolsKeyStorage));
    lua_pushnil(state);
    lua_rawset(state, LUA_REGISTRYINDEX);
    constexpr const char* globalKeys[] = {
        "Class",
        "class",
        "copy",
        "deepcopy",
        "super",
        "_LUDORK_RUNTIME_RESOLVER",
        "_LUDORK_STANDARD_UPDATE",
    };
    for (const char* key : globalKeys) {
        lua_pushnil(state);
        lua_setglobal(state, key);
    }
    lua_settop(state, stackTop);
}

}  // namespace ludork::standard::class_runtime
