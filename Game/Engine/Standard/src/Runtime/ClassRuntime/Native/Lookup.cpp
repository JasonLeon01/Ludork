#include "Native/NativeRuntime.hpp"
#include "Detail/RuntimeState.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

namespace ludork::standard::class_runtime::detail {

lua_glue::Object nativeObjectForType(lua_glue::StateView lua,
                                     const lua_glue::Table& fields,
                                     const lua_glue::Table& nativeType) {
    const lua_glue::Object rawObjects =
        fields.raw_get<lua_glue::Object>(protocol::NATIVE_OBJECTS_FIELD);
    if (!rawObjects.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    return rawObjects.as<lua_glue::Table>().raw_get<lua_glue::Object>(
        nativeTypeName(lua, nativeType));
}

lua_glue::Object cachedNativeMethod(lua_glue::StateView lua,
                                    lua_glue::Table fields,
                                    const lua_glue::Object& key,
                                    const lua_glue::Object& method,
                                    const lua_glue::Object& nativeObject,
                                    const lua_glue::Table& nativeType,
                                    bool objectMember) {
    const lua_glue::Object rawCache =
        fields.raw_get<lua_glue::Object>(NATIVE_METHOD_CACHE_FIELD);
    lua_glue::Table cache = rawCache.is<lua_glue::Table>()
                                ? rawCache.as<lua_glue::Table>()
                                : lua.create_table();
    if (!rawCache.is<lua_glue::Table>()) {
        fields.raw_set(NATIVE_METHOD_CACHE_FIELD, cache);
    }
    const lua_glue::Object rawEntry = cache.raw_get<lua_glue::Object>(key);
    if (rawEntry.is<lua_glue::Table>()) {
        const lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
        const lua_glue::Object cachedMethod =
            entry.raw_get<lua_glue::Object>(1);
        const lua_glue::Object cachedObject =
            entry.raw_get<lua_glue::Object>(2);
        const lua_glue::Object cachedWrapper =
            entry.raw_get<lua_glue::Object>(3);
        if (cachedWrapper.is<lua_glue::Function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedObject, nativeObject)) {
            return cachedWrapper;
        }
    }
    lua_glue::Table entry = lua.create_table();
    const lua_glue::Object wrapper =
        wrapNativeMethod(lua, method, nativeObject);
    entry.raw_set(1, method);
    entry.raw_set(2, nativeObject);
    entry.raw_set(3, wrapper);
    entry.raw_set(4, nativeType);
    entry.raw_set(5, objectMember);
    cache.raw_set(key, entry);
    return wrapper;
}

lua_glue::Object findCachedNativeMethod(lua_glue::StateView lua,
                                        lua_glue::Table fields,
                                        const lua_glue::Object& key) {
    const lua_glue::Object rawCache =
        fields.raw_get<lua_glue::Object>(NATIVE_METHOD_CACHE_FIELD);
    if (!rawCache.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    lua_glue::Table cache = rawCache.as<lua_glue::Table>();
    const lua_glue::Object rawEntry = cache.raw_get<lua_glue::Object>(key);
    if (!rawEntry.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    const lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
    const lua_glue::Object method = entry.raw_get<lua_glue::Object>(1);
    const lua_glue::Object nativeObject = entry.raw_get<lua_glue::Object>(2);
    const lua_glue::Object wrapper = entry.raw_get<lua_glue::Object>(3);
    const lua_glue::Object rawNativeType = entry.raw_get<lua_glue::Object>(4);
    const lua_glue::Object rawObjectMember = entry.raw_get<lua_glue::Object>(5);
    if (!method.is<lua_glue::Function>() ||
        (nativeObject.get_type() != lua_glue::Type::Userdata) ||
        !wrapper.is<lua_glue::Function>() ||
        !rawNativeType.is<lua_glue::Table>() || !rawObjectMember.is<bool>()) {
        cache.raw_set(key, lua_glue::nil);
        return nilObject(lua);
    }
    lua_glue::Object current = nilObject(lua);
    if (rawObjectMember.as<bool>()) {
        current = protectedIndex(lua, nativeObject, key);
    } else {
        current = rawMember(lua, rawNativeType.as<lua_glue::Table>(), key);
    }
    if (current.is<lua_glue::Function>() && objectsRawEqual(current, method)) {
        return wrapper;
    }
    cache.raw_set(key, lua_glue::nil);
    return nilObject(lua);
}

lua_Integer classLookupVersion(const lua_glue::Table& classTable) {
    const lua_glue::Object rawVersion =
        classTable.raw_get<lua_glue::Object>(LOOKUP_VERSION_FIELD);
    return rawVersion.is<lua_Integer>() ? rawVersion.as<lua_Integer>() : 0;
}

lua_glue::Table fastIndexCache(lua_glue::StateView lua,
                               lua_glue::Table fields) {
    const lua_glue::Object rawCache =
        fields.raw_get<lua_glue::Object>(FAST_INDEX_CACHE_FIELD);
    if (rawCache.is<lua_glue::Table>()) {
        return rawCache.as<lua_glue::Table>();
    }
    lua_glue::Table cache = lua.create_table();
    fields.raw_set(FAST_INDEX_CACHE_FIELD, cache);
    return cache;
}

void cacheFastIndex(lua_glue::StateView lua, lua_glue::Table fields,
                    const lua_glue::Table& classTable,
                    const lua_glue::Object& key, FastIndexKind kind,
                    const lua_glue::Object& route) {
    lua_glue::Table entry = lua.create_table(3, 0);
    entry.raw_set(1, static_cast<lua_Integer>(kind));
    entry.raw_set(2, route);
    entry.raw_set(3, classLookupVersion(classTable));
    fastIndexCache(lua, fields).raw_set(key, entry);
}

void cacheFastClassOwner(lua_glue::StateView lua, lua_glue::Table fields,
                         const lua_glue::Table& classTable,
                         const lua_glue::Object& key, const char* category,
                         FastIndexKind kind) {
    const lua_glue::Object rawOwner =
        classLookupOwners(lua, classTable, category)
            .raw_get<lua_glue::Object>(key);
    if (rawOwner.is<lua_glue::Table>()) {
        cacheFastIndex(lua, fields, classTable, key, kind, rawOwner);
    }
}

}  // namespace ludork::standard::class_runtime::detail
