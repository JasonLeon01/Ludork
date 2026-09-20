#include "Instance/InstanceRuntime.hpp"
#include <ClassRuntimeProtocol.hpp>
#include "Instance/LifecycleInternal.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaError.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Instance lifecycle
// ────────────────────────────────────────────────────────

namespace {

void reportDisposeError(const char* phase, const std::string& message) {
    std::fprintf(stderr, "Class dispose %s failed: %s\n", phase,
                 message.c_str());
}

bool tableMatchesInstanceClass(lua_glue::StateView lua,
                               const lua_glue::Table& instance,
                               const lua_glue::Table& classTable) {
    if (objectsRawEqual(instance, classTable)) {
        return false;
    }
    const lua_glue::Table metatable = class_native::getObjectMetatable(
        lua, lua_glue::MakeObject(lua, instance));
    if (objectsRawEqual(metatable, classTable)) {
        return true;
    }
    const lua_glue::Object rawMonitor =
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_get<lua_glue::Object>(instance);
    if (!rawMonitor.is<lua_glue::Table>()) {
        return false;
    }
    const lua_glue::Object originalMetatable =
        rawMonitor.as<lua_glue::Table>().raw_get<lua_glue::Object>("meta");
    return originalMetatable.is<lua_glue::Table>() &&
           objectsRawEqual(originalMetatable.as<lua_glue::Table>(), classTable);
}

LifecycleState lifecycleState(lua_glue::StateView lua,
                              const lua_glue::Object& instance) {
    const lua_glue::Object rawState =
        registryTable(lua, LIFECYCLE_STATES_KEY, "k")
            .raw_get<lua_glue::Object>(instance);
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

void setLifecycleState(lua_glue::StateView lua,
                       const lua_glue::Object& instance, LifecycleState state) {
    registryTable(lua, LIFECYCLE_STATES_KEY, "k")
        .raw_set(instance, static_cast<lua_Integer>(state));
}

int disposedInstanceAccess(lua_State* state) {
    return luaL_error(state, "Class instance has been disposed");
}

lua_glue::Table disposedInstanceMetatable(lua_glue::StateView lua) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object rawMetatable =
        registry.raw_get<lua_glue::Object>(DISPOSED_METATABLE_KEY);
    if (rawMetatable.is<lua_glue::Table>()) {
        return rawMetatable.as<lua_glue::Table>();
    }
    lua_glue::Table metatable = lua.create_table();
    metatable.push(lua.lua_state());
    lua_pushcfunction(lua.lua_state(), disposedInstanceAccess);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pushcfunction(lua.lua_state(), disposedInstanceAccess);
    lua_setfield(lua.lua_state(), -2, "__newindex");
    lua_pop(lua.lua_state(), 1);
    metatable.raw_set("__metatable", "disposed");
    registry.raw_set(DISPOSED_METATABLE_KEY, metatable);
    return metatable;
}

void protectDisposedInstance(lua_glue::StateView lua,
                             const lua_glue::Object& instance) {
    instance.push(lua.lua_state());
    disposedInstanceMetatable(lua).push(lua.lua_state());
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
}

int classInstanceDispose(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        if (lua_gettop(state) < 1) {
            throw std::invalid_argument("dispose requires a class instance");
        }
        lua_glue::StateView lua(state);
        disposeInstanceCore(lua, lua_glue::Read<lua_glue::Object>(state, 1),
                            true);
        return 0;
    });
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

void invokeClassDispose(lua_glue::StateView lua,
                        const lua_glue::Object& instance,
                        const lua_glue::Table& classTable) {
    const lua_glue::Object dispose =
        findScriptMember(lua, classTable, lua_glue::MakeObject(lua, "dispose"));
    if (!dispose.is<lua_glue::Function>()) {
        return;
    }
    dispose.push(lua.lua_state());
    instance.push(lua.lua_state());
    if (ludork::standard::protectedLuaCall(lua.lua_state(), 1, 0) != LUA_OK) {
        reportDisposeError("dispose", popLuaError(lua.lua_state(),
                                                  "Lua class dispose failed"));
    }
}

void clearInstanceMonitor(lua_glue::StateView lua,
                          const lua_glue::Object& instance) {
    lua_glue::Table states = registryTable(lua, MONITOR_STATES_KEY, "k");
    const lua_glue::Object rawMonitor =
        states.raw_get<lua_glue::Object>(instance);
    if (!instance.is<lua_glue::Table>() || !rawMonitor.is<lua_glue::Table>()) {
        states.raw_set(instance, lua_glue::nil);
        return;
    }
    const lua_glue::Object originalMetatable =
        rawMonitor.as<lua_glue::Table>().raw_get<lua_glue::Object>("meta");
    instance.push(lua.lua_state());
    if (originalMetatable.valid() &&
        originalMetatable.get_type() != lua_glue::Type::Nil) {
        originalMetatable.push(lua.lua_state());
    } else {
        lua_pushnil(lua.lua_state());
    }
    lua_setmetatable(lua.lua_state(), -2);
    lua_pop(lua.lua_state(), 1);
    states.raw_set(instance, lua_glue::nil);
}

DisposeSnapshot createDisposeSnapshot(lua_glue::StateView lua,
                                      const lua_glue::Object& instance) {
    const lua_glue::Table fields = managedInstanceFields(lua, instance);
    const lua_glue::Object rawClass =
        fields.raw_get<lua_glue::Object>(CLASS_FIELD);
    if (!rawClass.is<lua_glue::Table>()) {
        throw std::runtime_error("Class instance has no runtime class");
    }
    DisposeSnapshot snapshot{
        fields,
        rawClass.as<lua_glue::Table>(),
        std::nullopt,
        {},
    };
    const lua_glue::Object rawInstanceId =
        fields.raw_get<lua_glue::Object>(INSTANCE_ID_FIELD);
    if (rawInstanceId.is<std::size_t>()) {
        snapshot.instanceId = rawInstanceId.as<std::size_t>();
    }
    for (const lua_glue::Table& root : nativeRoots(lua, snapshot.classTable)) {
        const lua_glue::Object nativeObject =
            nativeObjectForType(lua, fields, root);
        if ((nativeObject.get_type() != lua_glue::Type::Userdata)) {
            continue;
        }
        const lua_glue::Object rawCallbacks =
            root.raw_get<lua_glue::Object>(CLASS_CALLBACKS_FIELD);
        const lua_glue::Object rawMetadataModule =
            root.raw_get<lua_glue::Object>(
                protocol::CLASS_METADATA_MODULE_FIELD);
        snapshot.nativeTargets.push_back({
            root,
            nativeObject,
            rawCallbacks.is<lua_glue::Table>() &&
                !tableIsEmpty(rawCallbacks.as<lua_glue::Table>()) &&
                rawMetadataModule.is<std::string>(),
        });
    }
    return snapshot;
}

void invokeNativeDisposeHooks(
    lua_glue::StateView lua, const lua_glue::Object& instance,
    const std::vector<DisposeSnapshot::NativeDisposeTarget>& targets) {
    for (const DisposeSnapshot::NativeDisposeTarget& target : targets) {
        runDisposePhase("native hook", [&]() {
            const lua_glue::Object rawDispose = rawMember(
                lua, target.root, lua_glue::MakeObject(lua, "__classRelease"));
            if (rawDispose.is<lua_glue::Function>()) {
                lua_glue::CallResult disposed =
                    rawDispose.as<lua_glue::Function>()(target.nativeObject);
                if (!disposed.valid()) {
                    const std::string error = disposed.error();
                    reportDisposeError("native hook", error.c_str());
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

void clearInstanceFields(lua_glue::Table fields) {
    std::vector<lua_glue::Object> keys;
    for (const auto& entry : fields) {
        keys.push_back(entry.first);
    }
    for (const lua_glue::Object& key : keys) {
        fields.raw_set(key, lua_glue::nil);
    }
}

}  // namespace

std::optional<lua_glue::Table> tryManagedInstanceFields(
    lua_glue::StateView lua, const lua_glue::Object& instance) {
    if (isCompositeInstance(lua, instance)) {
        const lua_glue::Table fields =
            class_native::getUserFields(lua, instance, false);
        const lua_glue::Object rawClass =
            fields.raw_get<lua_glue::Object>(CLASS_FIELD);
        if (rawClass.is<lua_glue::Table>() &&
            isClass(rawClass.as<lua_glue::Table>())) {
            return fields;
        }
    } else if (instance.is<lua_glue::Table>()) {
        const lua_glue::Table fields = instance.as<lua_glue::Table>();
        const lua_glue::Object rawClass =
            fields.raw_get<lua_glue::Object>(CLASS_FIELD);
        if (rawClass.is<lua_glue::Table>() &&
            isClass(rawClass.as<lua_glue::Table>()) &&
            tableMatchesInstanceClass(lua, fields,
                                      rawClass.as<lua_glue::Table>())) {
            return fields;
        }
    }
    return std::nullopt;
}

lua_glue::Table managedInstanceFields(lua_glue::StateView lua,
                                      const lua_glue::Object& instance) {
    const std::optional<lua_glue::Table> fields =
        tryManagedInstanceFields(lua, instance);
    if (fields.has_value()) {
        return *fields;
    }
    throw std::invalid_argument(
        "Class lifecycle requires a Ludork class instance");
}

lua_glue::Object instanceDisposeMethod(lua_glue::StateView lua,
                                       const lua_glue::Table& classTable,
                                       const lua_glue::Object& key) {
    if (!key.is<std::string>() || key.as<std::string>() != "dispose") {
        return nilObject(lua);
    }
    const lua_glue::Object dispose = findScriptMember(lua, classTable, key);
    if (!dispose.is<lua_glue::Function>()) {
        return nilObject(lua);
    }
    lua_pushcfunction(lua.lua_state(), classInstanceDispose);
    lua_glue::Object method =
        lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return method;
}

bool disposeInstanceCore(lua_glue::StateView lua,
                         const lua_glue::Object& instance, bool invokeDispose) {
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
                .raw_set(*snapshot.instanceId, lua_glue::nil);
        }
    });
    runDisposePhase("method cache", [&]() {
        clearNativeMethodCaches(lua, instance, snapshot.fields);
    });
    runDisposePhase("instance fields", [&]() {
        clearInstanceFields(snapshot.fields);
        clearExplicitNilFields(lua, instance);
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
            lua_glue::StateView lua(state);
            const lua_glue::Object shuttingDown =
                lua.registry().raw_get<lua_glue::Object>(SHUTTING_DOWN_KEY);
            if (!(shuttingDown.is<bool>() && shuttingDown.as<bool>())) {
                disposeInstanceCore(
                    lua, lua_glue::Read<lua_glue::Object>(state, 1), true);
            }
        }
    } catch (const std::exception& error) {
        reportDisposeError("__gc", error.what());
    } catch (...) {
        reportDisposeError("__gc", "unknown error");
    }
    return 0;
}

void failNativeConstruction(lua_glue::StateView lua,
                            const lua_glue::Table& classTable,
                            const lua_glue::Object& instance) {
    const lua_glue::Object rawClass = scriptClassOf(lua, instance);
    if (!rawClass.is<lua_glue::Table>() ||
        !objectsRawEqual(rawClass.as<lua_glue::Table>(), classTable)) {
        return;
    }
    runDisposePhase("failed construction", [&]() {
        disposeInstanceCore(lua, instance, false);
    });
}

}  // namespace ludork::standard::class_runtime::detail
