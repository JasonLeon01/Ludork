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
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>

namespace ludork::standard::class_runtime::detail {

// ── Read fast path
// ────────────────────────────────────────────────────────────

sol::object compositeIndexSlow(sol::object target, sol::object key,
                               sol::this_state state) {
    sol::state_view lua(state);
    const sol::table fields = class_native::getUserFields(lua, target, false);
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const sol::object field = fields.raw_get<sol::object>(key);
    if (field.valid() && field.get_type() != sol::type::lua_nil) {
        return field;
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table classTable = rawClass.as<sol::table>();
    const sol::object getter = findAccessor(lua, classTable, "__getters", key);
    if (getter.is<sol::function>()) {
        cacheFastClassOwner(lua, fields, classTable, key, "__getters",
                            FastIndexKind::Getter);
        return getter.as<sol::function>()(target);
    }
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        const sol::object nativeDefinition =
            nativeTypeDefinition(lua, nativeType, key);
        if (!nativeDefinition.valid() ||
            nativeDefinition.get_type() == sol::type::lua_nil) {
            continue;
        }
        if (!declaredProperty && !nativeDefinition.is<sol::function>()) {
            continue;
        }
        sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
        if (!nativeObject.is<sol::userdata>() && declaredProperty) {
            nativeObject = ensureDefaultNativeObject(lua, target, nativeType);
        }
        if (!nativeObject.is<sol::userdata>()) {
            continue;
        }
        const sol::object nativeValue = protectedIndex(lua, nativeObject, key);
        if (declaredProperty || !nativeValue.is<sol::function>()) {
            cacheFastIndex(
                lua, fields, classTable, key, FastIndexKind::NativeMember,
                sol::make_object(lua, nativeTypeName(lua, nativeType)));
            return nativeValue;
        }
    }
    const sol::object scriptMember = findScriptMember(lua, classTable, key);
    if (scriptMember.valid() && scriptMember.get_type() != sol::type::lua_nil) {
        cacheFastClassOwner(lua, fields, classTable, key, "scriptMembers",
                            FastIndexKind::ScriptMember);
        return scriptMember;
    }
    const sol::object cachedNative = findCachedNativeMethod(lua, fields, key);
    if (cachedNative.is<sol::function>()) {
        cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                       cachedNative);
        return cachedNative;
    }
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
        sol::object nativeValue = nilObject(lua);
        if (nativeObject.is<sol::userdata>()) {
            const sol::object nativeDefinition =
                nativeTypeDefinition(lua, nativeType, key);
            if (nativeDefinition.valid() &&
                nativeDefinition.get_type() != sol::type::lua_nil &&
                (declaredProperty || nativeDefinition.is<sol::function>())) {
                nativeValue = protectedIndex(lua, nativeObject, key);
                if (!nativeValue.is<sol::function>()) {
                    return nativeValue;
                }
            }
        }
        const sol::object member = rawMember(lua, nativeType, key);
        if (member.valid() && member.get_type() != sol::type::lua_nil) {
            if (isNativeInitializer(nativeType, member)) {
                const sol::object wrapper = cachedNativeMethod(
                    lua, fields, key, member, target, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            if (member.is<sol::function>()) {
                if (!nativeObject.is<sol::userdata>()) {
                    nativeObject =
                        ensureDefaultNativeObject(lua, target, nativeType);
                }
                const sol::object wrapper = cachedNativeMethod(
                    lua, fields, key, member, nativeObject, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            return member;
        }
        if (nativeValue.is<sol::function>()) {
            const sol::object wrapper = cachedNativeMethod(
                lua, fields, key, nativeValue, nativeObject, nativeType, true);
            cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                           wrapper);
            return wrapper;
        }
    }
    return nilObject(lua);
}

namespace {

int returnTopValue(lua_State* state) {
    lua_replace(state, 1);
    lua_settop(state, 1);
    return 1;
}

}  // namespace

void invalidateFastIndexEntry(lua_State* state, int cacheIndex) {
    lua_pushvalue(state, 2);
    lua_pushnil(state);
    lua_rawset(state, cacheIndex);
}

int compositeIndex(lua_State* state) {
    try {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);
            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool constructionFailed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (constructionFailed) {
                return luaL_error(state, "Class instance construction failed");
            }

            lua_pushvalue(state, 2);
            lua_rawget(state, fieldsIndex);
            if (!lua_isnil(state, -1)) {
                return returnTopValue(state);
            }
            lua_pop(state, 1);

            lua_getfield(state, fieldsIndex, FAST_INDEX_CACHE_FIELD);
            if (lua_istable(state, -1)) {
                const int cacheIndex = lua_absindex(state, -1);
                lua_pushvalue(state, 2);
                lua_rawget(state, cacheIndex);
                if (lua_istable(state, -1)) {
                    const int entryIndex = lua_absindex(state, -1);
                    lua_getfield(state, fieldsIndex, "__class");
                    if (lua_istable(state, -1)) {
                        lua_getfield(state, -1, "__lookupVersion");
                        const lua_Integer currentVersion =
                            lua_isinteger(state, -1) ? lua_tointeger(state, -1)
                                                     : 0;
                        lua_pop(state, 2);
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
                            if (kind == FastIndexKind::Value) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (!lua_isnil(state, -1)) {
                                    return returnTopValue(state);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::ScriptMember) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_pushvalue(state, 2);
                                    lua_rawget(state, -2);
                                    if (!lua_isnil(state, -1)) {
                                        return returnTopValue(state);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::Getter) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_getfield(state, -1, "__getters");
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            lua_call(state, 1, 1);
                                            return returnTopValue(state);
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::NativeMember) {
                                lua_getfield(state, fieldsIndex,
                                             protocol::NATIVE_OBJECTS_FIELD);
                                if (lua_istable(state, -1)) {
                                    lua_rawgeti(state, entryIndex, 2);
                                    lua_rawget(state, -2);
                                    if (lua_isuserdata(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_gettable(state, -2);
                                        if (!lua_isnil(state, -1)) {
                                            return returnTopValue(state);
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            }
                        }
                    } else {
                        lua_pop(state, 1);
                    }
                    invalidateFastIndexEntry(state, cacheIndex);
                }
            }
        }
        lua_settop(state, 2);
        sol::state_view lua(state);
        const sol::object target = sol::stack::get<sol::object>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object result = compositeIndexSlow(target, key, state);
        result.push();
        return returnTopValue(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

// ── Composite metatable creation
// ──────────────────────────────────────────────

namespace {

sol::table createCompositeMetatable(sol::state_view lua, const char* key,
                                    bool finalized) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable = registry.raw_get<sol::object>(key);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.raw_set(protocol::COMPOSITE_MARKER_FIELD, true);
    metatable.push();
    lua_pushcfunction(lua.lua_state(), compositeIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pushcfunction(lua.lua_state(), compositeNewIndex);
    lua_setfield(lua.lua_state(), -2, "__newindex");
    lua_pop(lua.lua_state(), 1);
    if (finalized) {
        metatable.push();
        lua_pushcfunction(lua.lua_state(), classInstanceGc);
        lua_setfield(lua.lua_state(), -2, "__gc");
        lua_pop(lua.lua_state(), 1);
    }
    registry.raw_set(key, metatable);
    return metatable;
}

}  // namespace

sol::table compositeMetatable(sol::state_view lua) {
    return createCompositeMetatable(lua, COMPOSITE_METATABLE_KEY, true);
}

sol::table constructingCompositeMetatable(sol::state_view lua) {
    return createCompositeMetatable(lua, CONSTRUCTING_COMPOSITE_METATABLE_KEY,
                                    false);
}

bool isCompositeInstance(sol::state_view lua, const sol::object& instance) {
    if (!instance.is<sol::userdata>()) {
        return false;
    }
    const sol::object marker =
        class_native::getObjectMetatable(lua, instance)
            .raw_get<sol::object>(protocol::COMPOSITE_MARKER_FIELD);
    return marker.is<bool>() && marker.as<bool>();
}

}  // namespace ludork::standard::class_runtime::detail
