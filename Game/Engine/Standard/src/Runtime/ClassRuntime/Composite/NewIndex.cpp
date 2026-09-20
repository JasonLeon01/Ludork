#include <LuaError.hpp>
#include "Composite/CompositeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

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

void compositeNewIndexSlow(lua_State* state, const lua_glue::Object& target,
                           const lua_glue::Object& key,
                           const lua_glue::Object& value) {
    lua_glue::StateView lua(state);
    lua_glue::Table fields = class_native::getUserFields(lua, target, true);
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const lua_glue::Object rawClass =
        fields.raw_get<lua_glue::Object>(CLASS_FIELD);
    auto assignValue = [&]() {
        if (!rawClass.is<lua_glue::Table>()) {
            fields.raw_set(key, value);
            clearExplicitNilField(lua, target, key);
            return;
        }
        const lua_glue::Table classTable = rawClass.as<lua_glue::Table>();
        const lua_glue::Object setter =
            findAccessor(lua, classTable, protocol::CLASS_SETTERS_FIELD, key);
        if (setter.is<lua_glue::Function>()) {
            const lua_glue::CallResult result =
                setter.as<lua_glue::Function>()(target, value);
            if (!result.valid()) {
                throw std::runtime_error(result.error());
            }
            clearExplicitNilField(lua, target, key);
            cacheFastClassOwner(lua, fields, classTable, key,
                                protocol::CLASS_GETTERS_FIELD,
                                FastIndexKind::Getter);
            return;
        }
        lua_glue::Object assignedObject = nilObject(lua);
        if (setNativeMember(lua, fields, classTable, key, value,
                            &assignedObject)) {
            clearExplicitNilField(lua, target, key);
            markNativePropertyDirty(lua, fields, assignedObject, key);
            const lua_glue::Object rawType = nativeTypeOf(lua, assignedObject);
            if (rawType.is<lua_glue::Table>()) {
                cacheFastIndex(
                    lua, fields, classTable, key, FastIndexKind::NativeMember,
                    lua_glue::MakeObject(
                        lua,
                        nativeTypeName(lua, rawType.as<lua_glue::Table>())));
            }
            return;
        }
        fields.raw_set(key, value);
        clearExplicitNilField(lua, target, key);
    };
    const lua_glue::Object rawCallbacks =
        fields.raw_get<lua_glue::Object>("__monitorCallbacks");
    if (!rawCallbacks.is<lua_glue::Table>()) {
        assignValue();
        return;
    }
    const lua_glue::Object rawEntry =
        rawCallbacks.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
    if (!rawEntry.is<lua_glue::Table>()) {
        assignValue();
        return;
    }
    lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
    if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    lua_glue::Object oldValue = compositeIndexSlow(target, key, state);
    if (!oldValue.valid() || oldValue.get_type() == lua_glue::Type::Nil) {
        oldValue = entry.raw_get<lua_glue::Object>("missing");
    }
    assignValue();
    invokeMonitorCallbacks(lua, entry, oldValue, value);
}

}  // namespace

// C fast path: NativeMember and Getter cache hits use the Lua stack directly.
// Degrades to compositeNewIndexSlow for monitors, cache misses, or
// initializing state (where dirty tracking is needed).
int compositeNewIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);

            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool failed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (failed) {
                throw std::invalid_argument(
                    "Class instance construction failed");
            }

            // Monitored writes need per-subscription dispatch and validation.
            bool activeMonitor = false;
            lua_getfield(state, fieldsIndex, "__monitorCallbacks");
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (lua_istable(state, -1)) {
                    activeMonitor = true;
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
                        lua_getfield(state, fieldsIndex, CLASS_FIELD);
                        lua_Integer currentVersion = 0;
                        if (lua_istable(state, -1)) {
                            lua_getfield(state, -1, LOOKUP_VERSION_FIELD);
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
                                            clearExplicitNilField(state, 1, 2);
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
                                    lua_getfield(state, -1,
                                                 protocol::CLASS_SETTERS_FIELD);
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            lua_pushvalue(state, 3);
                                            if (ludork::standard::
                                                    protectedLuaCall(state, 2,
                                                                     0) !=
                                                LUA_OK) {
                                                throw std::runtime_error(
                                                    ludork::standard::
                                                        luaErrorMessage(state,
                                                                        -1));
                                            }
                                            clearExplicitNilField(state, 1, 2);
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
        lua_glue::StateView lua(state);
        compositeNewIndexSlow(state, lua_glue::Read<lua_glue::Object>(state, 1),
                              lua_glue::Read<lua_glue::Object>(state, 2),
                              lua_glue::Read<lua_glue::Object>(state, 3));
        return 0;
    });
}

}  // namespace ludork::standard::class_runtime::detail
