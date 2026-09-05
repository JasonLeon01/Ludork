#include "Instance/InstanceRuntime.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaError.hpp>
#include <sol2/sol.hpp>

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

bool tableMatchesInstanceClass(sol::state_view lua, const sol::table& instance,
                               const sol::table& classTable) {
    if (objectsRawEqual(instance, classTable)) {
        return false;
    }
    const sol::table metatable =
        class_native::getObjectMetatable(lua, sol::make_object(lua, instance));
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

}  // namespace

std::optional<sol::table> tryManagedInstanceFields(
    sol::state_view lua, const sol::object& instance) {
    if (isCompositeInstance(lua, instance)) {
        const sol::table fields =
            class_native::getUserFields(lua, instance, false);
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

namespace {

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

}  // namespace

void reportDisposeError(const char* phase, const std::string& message) {
    std::fprintf(stderr, "Class dispose %s failed: %s\n", phase,
                 message.c_str());
}

namespace {

int classInstanceDispose(lua_State* state) {
    try {
        if (lua_gettop(state) < 1) {
            return luaL_error(state, "dispose requires a class instance");
        }
        sol::state_view lua(state);
        disposeInstanceCore(lua, sol::stack::get<sol::object>(state, 1), true);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
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

}  // namespace

sol::object instanceDisposeMethod(sol::state_view lua,
                                  const sol::table& classTable,
                                  const sol::object& key) {
    if (!key.is<std::string>() || key.as<std::string>() != "dispose") {
        return nilObject(lua);
    }
    const sol::object dispose = findScriptMember(lua, classTable, key);
    if (!dispose.is<sol::function>()) {
        return nilObject(lua);
    }
    lua_pushcfunction(lua.lua_state(), classInstanceDispose);
    sol::object method = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return method;
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

}  // namespace ludork::standard::class_runtime::detail
