#include "Native/NativeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypedFields.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <stdexcept>

namespace ludork::standard::class_runtime::detail {

lua_glue::Object rawMember(lua_glue::StateView lua, const lua_glue::Table& type,
                           const lua_glue::Object& key) {
    lua_glue::Object result = type.raw_get<lua_glue::Object>(key);
    if ((!result.valid() || result.get_type() == lua_glue::Type::Nil) &&
        !isClass(type)) {
        if (nativeClassProperty(lua, type, key, result)) {
            return result;
        }
        const lua_glue::Object rawIndex =
            class_native::getObjectMetatable(lua,
                                             lua_glue::MakeObject(lua, type))
                .raw_get<lua_glue::Object>("__index");
        if (rawIndex.is<lua_glue::Function>()) {
            lua_glue::CallResult indexed =
                rawIndex.as<lua_glue::Function>()(type, key);
            if (indexed.valid()) {
                result = indexed.get<lua_glue::Object>();
            }
        }
    }
    return result.valid() ? result : nilObject(lua);
}

namespace {

bool pushPresentMember(lua_State* state, int ownerIndex, int keyIndex) {
    lua_pushvalue(state, keyIndex);
    lua_rawget(state, ownerIndex);
    if (lua_isnil(state, -1) && !isClass(state, ownerIndex)) {
        lua_pop(state, 1);
        rawMember(lua_glue::StateView(state),
                  lua_glue::Table(state, ownerIndex),
                  lua_glue::Object(state, keyIndex))
            .push(state);
    }
    return !lua_isnil(state, -1) ||
           hasExplicitNilField(state, ownerIndex, keyIndex);
}

}  // namespace

void pushClassMember(lua_State* state, int classIndex, int keyIndex,
                     bool includeClass) {
    if (!lua_checkstack(state, 12)) {
        throw std::runtime_error("Class lookup stack cannot grow");
    }
    classIndex = lua_absindex(state, classIndex);
    keyIndex = lua_absindex(state, keyIndex);
    pushClassLookupOwners(state, classIndex,
                          includeClass ? "members" : "baseMembers");
    const int ownersIndex = lua_gettop(state);
    lua_pushvalue(state, keyIndex);
    lua_rawget(state, ownersIndex);
    if (lua_istable(state, -1)) {
        if (pushPresentMember(state, lua_gettop(state), keyIndex)) {
            lua_replace(state, ownersIndex);
            lua_settop(state, ownersIndex);
            return;
        }
        lua_pushvalue(state, keyIndex);
        lua_pushnil(state);
        lua_rawset(state, ownersIndex);
    }
    lua_settop(state, ownersIndex);
    pushClassLookupMro(state, classIndex);
    const int mroIndex = lua_gettop(state);
    for (lua_Integer index = includeClass ? 1 : 2;
         index <= static_cast<lua_Integer>(lua_rawlen(state, mroIndex));
         ++index) {
        lua_geti(state, mroIndex, index);
        const int ownerIndex = lua_gettop(state);
        if (lua_istable(state, ownerIndex) &&
            pushPresentMember(state, ownerIndex, keyIndex)) {
            lua_pushvalue(state, keyIndex);
            lua_pushvalue(state, ownerIndex);
            lua_rawset(state, ownersIndex);
            lua_replace(state, ownersIndex);
            lua_settop(state, ownersIndex);
            return;
        }
        lua_settop(state, mroIndex);
    }
    lua_settop(state, ownersIndex - 1);
    lua_pushnil(state);
}

}  // namespace ludork::standard::class_runtime::detail
