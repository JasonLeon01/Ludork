#include <LuaError.hpp>
#include "Composite/CompositeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Instance/InstanceRuntime.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

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

lua_glue::Object compositeIndexSlow(lua_glue::Object target,
                                    lua_glue::Object key,
                                    lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    const lua_glue::Table fields =
        class_native::getUserFields(lua, target, false);
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const lua_glue::Object field = fields.raw_get<lua_glue::Object>(key);
    if (field.valid() && field.get_type() != lua_glue::Type::Nil) {
        return field;
    }
    const lua_glue::Object rawClass =
        fields.raw_get<lua_glue::Object>(CLASS_FIELD);
    if (!rawClass.is<lua_glue::Table>()) {
        return nilObject(lua);
    }
    const lua_glue::Table classTable = rawClass.as<lua_glue::Table>();
    const lua_glue::Object disposeMethod =
        instanceDisposeMethod(lua, classTable, key);
    if (disposeMethod.is<lua_glue::Function>()) {
        cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                       disposeMethod);
        return disposeMethod;
    }
    const lua_glue::Object getter =
        findAccessor(lua, classTable, protocol::CLASS_GETTERS_FIELD, key);
    if (getter.is<lua_glue::Function>()) {
        cacheFastClassOwner(lua, fields, classTable, key,
                            protocol::CLASS_GETTERS_FIELD,
                            FastIndexKind::Getter);
        return getter.as<lua_glue::Function>()(target);
    }
    const lua_glue::Table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        const lua_glue::Object nativeDefinition =
            nativeTypeDefinition(lua, nativeType, key);
        if (!nativeDefinition.valid() ||
            nativeDefinition.get_type() == lua_glue::Type::Nil) {
            continue;
        }
        if (!declaredProperty && !nativeDefinition.is<lua_glue::Function>()) {
            continue;
        }
        lua_glue::Object nativeObject =
            nativeObjectForType(lua, fields, nativeType);
        if ((nativeObject.get_type() != lua_glue::Type::Userdata) &&
            declaredProperty) {
            nativeObject = ensureDefaultNativeObject(lua, target, nativeType);
        }
        if ((nativeObject.get_type() != lua_glue::Type::Userdata)) {
            continue;
        }
        const lua_glue::Object nativeValue =
            protectedIndex(lua, nativeObject, key);
        if (declaredProperty || !nativeValue.is<lua_glue::Function>()) {
            cacheFastIndex(
                lua, fields, classTable, key, FastIndexKind::NativeMember,
                lua_glue::MakeObject(lua, nativeTypeName(lua, nativeType)));
            return nativeValue;
        }
    }
    if (hasExplicitNilField(lua, target, key)) {
        return nilObject(lua);
    }
    bool foundScriptMember = false;
    const lua_glue::Object scriptMember =
        findScriptMember(lua, classTable, key, &foundScriptMember);
    if (foundScriptMember) {
        cacheFastClassOwner(lua, fields, classTable, key, "scriptMembers",
                            FastIndexKind::ScriptMember);
        return scriptMember;
    }
    const lua_glue::Object cachedNative =
        findCachedNativeMethod(lua, fields, key);
    if (cachedNative.is<lua_glue::Function>()) {
        cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                       cachedNative);
        return cachedNative;
    }
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        lua_glue::Object nativeObject =
            nativeObjectForType(lua, fields, nativeType);
        lua_glue::Object nativeValue = nilObject(lua);
        if ((nativeObject.get_type() == lua_glue::Type::Userdata)) {
            const lua_glue::Object nativeDefinition =
                nativeTypeDefinition(lua, nativeType, key);
            if (nativeDefinition.valid() &&
                nativeDefinition.get_type() != lua_glue::Type::Nil &&
                (declaredProperty ||
                 nativeDefinition.is<lua_glue::Function>())) {
                nativeValue = protectedIndex(lua, nativeObject, key);
                if (!nativeValue.is<lua_glue::Function>()) {
                    return nativeValue;
                }
            }
        }
        const lua_glue::Object member = rawMember(lua, nativeType, key);
        if (member.valid() && member.get_type() != lua_glue::Type::Nil) {
            if (isNativeInitializer(nativeType, member)) {
                const lua_glue::Object wrapper = cachedNativeMethod(
                    lua, fields, key, member, target, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            if (member.is<lua_glue::Function>()) {
                if ((nativeObject.get_type() != lua_glue::Type::Userdata)) {
                    nativeObject =
                        ensureDefaultNativeObject(lua, target, nativeType);
                }
                const lua_glue::Object wrapper = cachedNativeMethod(
                    lua, fields, key, member, nativeObject, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            return member;
        }
        if (nativeValue.is<lua_glue::Function>()) {
            const lua_glue::Object wrapper = cachedNativeMethod(
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

lua_glue::Table createCompositeMetatable(lua_glue::StateView lua,
                                         const char* key, bool finalized) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object rawMetatable =
        registry.raw_get<lua_glue::Object>(key);
    if (rawMetatable.is<lua_glue::Table>()) {
        return rawMetatable.as<lua_glue::Table>();
    }
    lua_glue::Table metatable = lua.create_table();
    metatable.raw_set(protocol::COMPOSITE_MARKER_FIELD, true);
    metatable.push(lua.lua_state());
    lua_pushcfunction(lua.lua_state(), compositeIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pushcfunction(lua.lua_state(), compositeNewIndex);
    lua_setfield(lua.lua_state(), -2, "__newindex");
    lua_pop(lua.lua_state(), 1);
    if (finalized) {
        metatable.push(lua.lua_state());
        lua_pushcfunction(lua.lua_state(), classInstanceGc);
        lua_setfield(lua.lua_state(), -2, "__gc");
        lua_pop(lua.lua_state(), 1);
    }
    registry.raw_set(key, metatable);
    return metatable;
}

}  // namespace

void invalidateFastIndexEntry(lua_State* state, int cacheIndex) {
    lua_pushvalue(state, 2);
    lua_pushnil(state);
    lua_rawset(state, cacheIndex);
}

int compositeIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);
            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool constructionFailed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (constructionFailed) {
                throw std::invalid_argument(
                    "Class instance construction failed");
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
                    lua_getfield(state, fieldsIndex, CLASS_FIELD);
                    if (lua_istable(state, -1)) {
                        lua_getfield(state, -1, LOOKUP_VERSION_FIELD);
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
                            if ((kind == FastIndexKind::Value ||
                                 kind == FastIndexKind::ScriptMember) &&
                                hasExplicitNilField(state, 1, 2)) {
                                lua_pushnil(state);
                                return returnTopValue(state);
                            }
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
                                    if (!lua_isnil(state, -1) ||
                                        hasExplicitNilField(state, -2, 2)) {
                                        return returnTopValue(state);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::Getter) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_getfield(state, -1,
                                                 protocol::CLASS_GETTERS_FIELD);
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            if (ludork::standard::
                                                    protectedLuaCall(state, 1,
                                                                     1) !=
                                                LUA_OK) {
                                                throw std::runtime_error(
                                                    ludork::standard::
                                                        luaErrorMessage(state,
                                                                        -1));
                                            }
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
        lua_glue::StateView lua(state);
        const lua_glue::Object target =
            lua_glue::Read<lua_glue::Object>(state, 1);
        const lua_glue::Object key = lua_glue::Read<lua_glue::Object>(state, 2);
        const lua_glue::Object result = compositeIndexSlow(target, key, state);
        result.push(state);
        return returnTopValue(state);
    });
}

// ── Composite metatable creation
// ──────────────────────────────────────────────

lua_glue::Table compositeMetatable(lua_glue::StateView lua) {
    return createCompositeMetatable(lua, COMPOSITE_METATABLE_KEY, true);
}

lua_glue::Table constructingCompositeMetatable(lua_glue::StateView lua) {
    return createCompositeMetatable(lua, CONSTRUCTING_COMPOSITE_METATABLE_KEY,
                                    false);
}

}  // namespace ludork::standard::class_runtime::detail
