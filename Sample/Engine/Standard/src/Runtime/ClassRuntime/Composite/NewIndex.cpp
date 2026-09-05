#include "Composite/CompositeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <stdexcept>

namespace ludork::standard::class_runtime::detail {

// ── Write fast path (task 1)
// ──────────────────────────────────────────────────

// Slow write path: handles setter, native member, plain field, and monitor
// callbacks. Does NOT clear FAST_INDEX_CACHE; instead populates it on success.
namespace {

void compositeNewIndexSlow(lua_State* state, const sol::object& target,
                           const sol::object& key, const sol::object& value) {
    sol::state_view lua(state);
    sol::table fields = class_native::getUserFields(lua, target, true);
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
            cacheFastClassOwner(lua, fields, classTable, key, "__getters",
                                FastIndexKind::Getter);
            return;
        }
        sol::object assignedObject = nilObject(lua);
        if (setNativeMember(lua, fields, classTable, key, value,
                            &assignedObject)) {
            markNativePropertyDirty(lua, fields, assignedObject, key);
            const sol::object rawType = nativeTypeOf(lua, assignedObject);
            if (rawType.is<sol::table>()) {
                cacheFastIndex(
                    lua, fields, classTable, key, FastIndexKind::NativeMember,
                    sol::make_object(
                        lua, nativeTypeName(lua, rawType.as<sol::table>())));
            }
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

}  // namespace

// C fast path: NativeMember and Getter cache hits bypass sol entirely.
// Degrades to compositeNewIndexSlow for monitors, cache misses, or
// initializing state (where dirty tracking is needed).
int compositeNewIndex(lua_State* state) {
    try {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);

            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool failed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (failed) {
                return luaL_error(state, "Class instance construction failed");
            }

            // Skip fast path when there is an active (non-running) monitor
            bool activeMonitor = false;
            lua_getfield(state, fieldsIndex, "__monitorCallbacks");
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (lua_istable(state, -1)) {
                    lua_getfield(state, -1, "running");
                    activeMonitor = !lua_toboolean(state, -1);
                    lua_pop(state, 1);
                }
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            if (!activeMonitor) {
                lua_getfield(state, fieldsIndex, FAST_INDEX_CACHE_FIELD);
                if (lua_istable(state, -1)) {
                    const int cacheIndex = lua_absindex(state, -1);
                    lua_pushvalue(state, 2);
                    lua_rawget(state, cacheIndex);
                    if (lua_istable(state, -1)) {
                        const int entryIndex = lua_absindex(state, -1);
                        lua_getfield(state, fieldsIndex, "__class");
                        lua_Integer currentVersion = 0;
                        if (lua_istable(state, -1)) {
                            lua_getfield(state, -1, "__lookupVersion");
                            currentVersion = lua_isinteger(state, -1)
                                                 ? lua_tointeger(state, -1)
                                                 : 0;
                            lua_pop(state, 1);
                        }
                        lua_pop(state, 1);
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
                            if (kind == FastIndexKind::NativeMember) {
                                // Skip during initialisation (needs dirty
                                // tracking)
                                lua_getfield(state, fieldsIndex,
                                             NATIVE_INITIALIZING_FIELD);
                                const bool initializing =
                                    lua_toboolean(state, -1) != 0;
                                lua_pop(state, 1);
                                if (!initializing) {
                                    lua_getfield(
                                        state, fieldsIndex,
                                        protocol::NATIVE_OBJECTS_FIELD);
                                    if (lua_istable(state, -1)) {
                                        const int objectsIdx =
                                            lua_absindex(state, -1);
                                        lua_rawgeti(state, entryIndex, 2);
                                        lua_rawget(state, objectsIdx);
                                        if (lua_isuserdata(state, -1)) {
                                            lua_pushvalue(state, 2);
                                            lua_pushvalue(state, 3);
                                            lua_settable(state, -3);
                                            lua_settop(state, 0);
                                            return 0;
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                invalidateFastIndexEntry(state, cacheIndex);
                            } else if (kind == FastIndexKind::Getter) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_getfield(state, -1, "__setters");
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            lua_pushvalue(state, 3);
                                            lua_call(state, 2, 0);
                                            lua_settop(state, 0);
                                            return 0;
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            }
                        } else {
                            invalidateFastIndexEntry(state, cacheIndex);
                        }
                    }
                    lua_pop(state, 1);
                }
                lua_pop(state, 1);
            }
        }
        lua_settop(state, 3);
        sol::state_view lua(state);
        compositeNewIndexSlow(state, sol::stack::get<sol::object>(state, 1),
                              sol::stack::get<sol::object>(state, 2),
                              sol::stack::get<sol::object>(state, 3));
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

}  // namespace ludork::standard::class_runtime::detail
