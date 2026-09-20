#include "Class/ClassRuntimeInternals.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Instance/InstanceRuntime.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ludork::standard::class_runtime::detail {

void finishNativeConstruction(lua_glue::StateView lua,
                              const lua_glue::Table& classTable,
                              const lua_glue::Object& instance) {
    if (!compositeBelongsToClass(lua, instance, classTable)) {
        return;
    }
    lua_glue::Table fields = class_native::getUserFields(lua, instance, false);
    fields.raw_set(NATIVE_INITIALIZING_FIELD, false);
    fields.raw_set(NATIVE_CONSTRUCTION_FAILED_FIELD, lua_glue::nil);
    fields.raw_set(CLASS_INITIALIZED_ROOTS_FIELD, lua_glue::nil);
    fields.raw_set(NATIVE_CONSTRUCTING_ROOTS_FIELD, lua_glue::nil);
    fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, lua_glue::nil);
    instance.push(lua.lua_state());
    compositeMetatable(lua).push(lua.lua_state());
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
}

namespace {

lua_glue::Object createNativeInstance(
    lua_glue::StateView lua, const lua_glue::Table& classTable,
    const lua_glue::Object& rawConstructorArguments, bool allowDeferredRoots) {
    const std::vector<lua_glue::Table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return nilObject(lua);
    }
    lua_glue::Table constructorArguments = lua.create_table();
    if (rawConstructorArguments.valid() &&
        rawConstructorArguments.get_type() != lua_glue::Type::Nil) {
        if (!rawConstructorArguments.is<lua_glue::Table>()) {
            throw std::invalid_argument(
                "Class allocator expects a native constructor argument map");
        }
        constructorArguments = rawConstructorArguments.as<lua_glue::Table>();
        for (const auto& entry : constructorArguments) {
            if (!entry.first.is<lua_glue::Table>()) {
                throw std::invalid_argument(
                    "Native constructor map keys must be native root types");
            }
            bool knownRoot = false;
            for (const lua_glue::Table& root : roots) {
                if (objectsRawEqual(entry.first.as<lua_glue::Table>(), root)) {
                    knownRoot = true;
                    break;
                }
            }
            if (!knownRoot) {
                throw std::invalid_argument(
                    "Native constructor map contains a type that is not a "
                    "native root");
            }
            if (!entry.second.is<lua_glue::Table>()) {
                throw std::invalid_argument(
                    "Native constructor map values must be packed argument "
                    "tables");
            }
        }
    }
    lua_glue::Table fields = lua.create_table();
    lua_glue::Table nativeObjects = lua.create_table();
    fields.raw_set(CLASS_FIELD, classTable);
    fields.raw_set(protocol::NATIVE_OBJECTS_FIELD, nativeObjects);
    const std::size_t instanceId = class_native::nextInstanceId(lua);
    fields.raw_set(INSTANCE_ID_FIELD, instanceId);
    fields.raw_set(NATIVE_INITIALIZING_FIELD, true);
    lua_newuserdatauv(lua.lua_state(), 1, 1);
    constructingCompositeMetatable(lua).push(lua.lua_state());
    lua_setmetatable(lua.lua_state(), -2);
    fields.push(lua.lua_state());
    lua_setiuservalue(lua.lua_state(), -2, 1);
    lua_glue::Object instance =
        lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    registryTable(lua, INSTANCES_KEY, "v").raw_set(instanceId, instance);
    try {
        for (const lua_glue::Table& root : roots) {
            const lua_glue::Object arguments =
                constructorArguments.raw_get<lua_glue::Object>(root);
            if (allowDeferredRoots && !arguments.is<lua_glue::Table>() &&
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

lua_glue::Object allocateInstance(lua_glue::StateView lua,
                                  const lua_glue::Table& classTable,
                                  const lua_glue::Object& constructorArguments,
                                  bool allowDeferredRoots) {
    lua_glue::Object instance = createNativeInstance(
        lua, classTable, constructorArguments, allowDeferredRoots);
    if (!instance.valid() || instance.get_type() == lua_glue::Type::Nil) {
        lua_glue::Table tableInstance = lua.create_table();
        tableInstance.raw_set(CLASS_FIELD, classTable);
        lua_glue::SetMetatable(tableInstance, classTable);
        instance = tableInstance;
    }
    return instance;
}

}  // namespace ludork::standard::class_runtime::detail
