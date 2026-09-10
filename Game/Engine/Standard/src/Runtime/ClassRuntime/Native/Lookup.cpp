#include "Native/NativeRuntime.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

namespace ludork::standard::class_runtime::detail {

sol::object nativeObjectForType(sol::state_view lua, const sol::table& fields,
                                const sol::table& nativeType) {
    const sol::object rawObjects =
        fields.raw_get<sol::object>(protocol::NATIVE_OBJECTS_FIELD);
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

}  // namespace ludork::standard::class_runtime::detail
