#include "Instance/InstanceRuntime.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ludork::standard::class_runtime::detail {

void finishNativeConstruction(sol::state_view lua, const sol::table& classTable,
                              const sol::object& instance) {
    if (!compositeBelongsToClass(lua, instance, classTable)) {
        return;
    }
    sol::table fields = class_native::getUserFields(lua, instance, false);
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

// ── Instance allocation
// ───────────────────────────────────────────────────────

bool nativeRootIsDeferred(const sol::table& root) {
    const sol::object rawMinimum =
        root.raw_get<sol::object>("__classFactoryMinArgs");
    return rawMinimum.is<lua_Integer>() && rawMinimum.as<lua_Integer>() >= 0;
}

namespace {

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
    fields.raw_set(protocol::NATIVE_OBJECTS_FIELD, nativeObjects);
    const std::size_t instanceId = class_native::nextInstanceId(lua);
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

}  // namespace

sol::object allocateInstance(sol::state_view lua, const sol::table& classTable,
                             const sol::object& constructorArguments,
                             bool allowDeferredRoots) {
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

}  // namespace ludork::standard::class_runtime::detail
