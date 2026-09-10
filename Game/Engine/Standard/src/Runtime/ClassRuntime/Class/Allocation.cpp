#include "Class/ClassRuntimeInternals.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Instance/InstanceRuntime.hpp"
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
