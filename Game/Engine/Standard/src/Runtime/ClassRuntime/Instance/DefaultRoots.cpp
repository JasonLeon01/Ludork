#include "Instance/InstanceRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>

namespace ludork::standard::class_runtime::detail {

void completeDefaultNativeRoots(lua_glue::StateView lua,
                                const lua_glue::Table& classTable,
                                const lua_glue::Object& instance) {
    if (!isCompositeInstance(lua, instance)) {
        return;
    }
    lua_glue::Table fields = class_native::getUserFields(lua, instance, false);
    for (const lua_glue::Table& root : nativeRoots(lua, classTable)) {
        if ((nativeObjectForType(lua, fields, root).get_type() ==
             lua_glue::Type::Userdata)) {
            continue;
        }
        const lua_glue::Object rawMinimum =
            root.raw_get<lua_glue::Object>(CLASS_FACTORY_MIN_ARGUMENTS_FIELD);
        if (!rawMinimum.is<lua_Integer>() ||
            rawMinimum.as<lua_Integer>() != 0) {
            continue;
        }
        constructNativeRoot(lua, classTable, instance, root, nilObject(lua));
    }
}

lua_glue::Object ensureDefaultNativeObject(lua_glue::StateView lua,
                                           const lua_glue::Object& instance,
                                           const lua_glue::Table& nativeType) {
    if (!isCompositeInstance(lua, instance)) {
        return nilObject(lua);
    }
    lua_glue::Table fields = class_native::getUserFields(lua, instance, false);
    lua_glue::Object nativeObject =
        nativeObjectForType(lua, fields, nativeType);
    if ((nativeObject.get_type() == lua_glue::Type::Userdata)) {
        return nativeObject;
    }
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return nilObject(lua);
    }
    const lua_glue::Object rawClass =
        fields.raw_get<lua_glue::Object>(CLASS_FIELD);
    const lua_glue::Object rawObjects =
        fields.raw_get<lua_glue::Object>(protocol::NATIVE_OBJECTS_FIELD);
    const lua_glue::Object rawInstanceId =
        fields.raw_get<lua_glue::Object>(INSTANCE_ID_FIELD);
    if (!rawClass.is<lua_glue::Table>() || !rawObjects.is<lua_glue::Table>() ||
        !rawInstanceId.is<std::size_t>()) {
        return nilObject(lua);
    }
    const lua_glue::Table classTable = rawClass.as<lua_glue::Table>();
    lua_glue::Table root = lua.create_table();
    bool foundRoot = false;
    for (const lua_glue::Table& candidate : nativeRoots(lua, classTable)) {
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
    const lua_glue::Object rawMinimum =
        root.raw_get<lua_glue::Object>(CLASS_FACTORY_MIN_ARGUMENTS_FIELD);
    if (!rawMinimum.is<lua_Integer>() || rawMinimum.as<lua_Integer>() != 0) {
        return nilObject(lua);
    }
    constructNativeRoot(lua, classTable, instance, root, nilObject(lua));
    return nativeObjectForType(lua, fields, nativeType);
}

bool nativeRootIsDeferred(const lua_glue::Table& root) {
    const lua_glue::Object rawMinimum =
        root.raw_get<lua_glue::Object>(CLASS_FACTORY_MIN_ARGUMENTS_FIELD);
    return rawMinimum.is<lua_Integer>() && rawMinimum.as<lua_Integer>() >= 0;
}

}  // namespace ludork::standard::class_runtime::detail
