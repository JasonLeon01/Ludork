#include "Instance/InstanceRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>

namespace ludork::standard::class_runtime::detail {

void completeDefaultNativeRoots(sol::state_view lua,
                                const sol::table& classTable,
                                const sol::object& instance) {
    if (!isCompositeInstance(lua, instance)) {
        return;
    }
    sol::table fields = class_native::getUserFields(lua, instance, false);
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
    sol::table fields = class_native::getUserFields(lua, instance, false);
    sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
    if (nativeObject.is<sol::userdata>()) {
        return nativeObject;
    }
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return nilObject(lua);
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    const sol::object rawObjects =
        fields.raw_get<sol::object>(protocol::NATIVE_OBJECTS_FIELD);
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

bool nativeRootIsDeferred(const sol::table& root) {
    const sol::object rawMinimum =
        root.raw_get<sol::object>("__classFactoryMinArgs");
    return rawMinimum.is<lua_Integer>() && rawMinimum.as<lua_Integer>() >= 0;
}

}  // namespace ludork::standard::class_runtime::detail
