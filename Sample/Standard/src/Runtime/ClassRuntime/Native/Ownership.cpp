#include "Native/NativeRuntime.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>

namespace ludork::standard::class_runtime::detail {

bool nativeTypeAccepts(sol::state_view lua, const sol::table& nativeType,
                       const sol::object& value) {
    const sol::object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<sol::table>()) {
        return false;
    }
    const sol::object rawIs =
        rawTypeInfo.as<sol::table>().raw_get<sol::object>("is");
    if (!rawIs.is<sol::protected_function>()) {
        return false;
    }
    sol::protected_function_result result =
        rawIs.as<sol::protected_function>()(value);
    return result.valid() && result.get_type() == sol::type::boolean &&
           result.get<bool>();
}

void registerMethodOwner(sol::state_view lua, const sol::table& classTable,
                         const sol::object& value) {
    if (!value.is<sol::function>()) {
        return;
    }
    registryTable(lua, METHOD_OWNERS_KEY, "k").raw_set(value, classTable);
}

namespace {

void* nativePointer(lua_State* state, int index) {
    const int absoluteIndex = lua_absindex(state, index);
    if (lua_type(state, absoluteIndex) != LUA_TUSERDATA ||
        lua_getmetatable(state, absoluteIndex) == 0) {
        return nullptr;
    }
    lua_getfield(state, -1, protocol::COMPOSITE_MARKER_FIELD);
    const bool composite = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    lua_getfield(state, -1, "__type");
    const bool native = lua_istable(state, -1);
    lua_pop(state, 2);
    if (composite || !native) {
        return nullptr;
    }
    void* memory = lua_touserdata(state, absoluteIndex);
    if (memory == nullptr) {
        return nullptr;
    }
    void* rawData = sol::detail::align_usertype_pointer(memory);
    return *static_cast<void**>(rawData);
}

}  // namespace

void registerNativePointerOwner(sol::state_view lua,
                                const sol::object& nativeObject,
                                const sol::object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push();
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    registryTable(lua, protocol::NATIVE_POINTER_OWNERS_REGISTRY_KEY, "v")
        .push();
    lua_pushlightuserdata(state, pointer);
    owner.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

void unregisterNativePointerOwner(sol::state_view lua,
                                  const sol::object& nativeObject,
                                  const sol::object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push();
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    sol::table owners =
        registryTable(lua, protocol::NATIVE_POINTER_OWNERS_REGISTRY_KEY, "v");
    owners.push();
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, ownersIndex);
    owner.push();
    const bool matches = lua_rawequal(state, -1, -2) != 0;
    lua_pop(state, 2);
    if (!matches) {
        lua_pop(state, 1);
        return;
    }
    lua_pushlightuserdata(state, pointer);
    lua_pushnil(state);
    lua_rawset(state, ownersIndex);
    lua_pop(state, 1);
}

bool pushNativeOwner(lua_State* state, int nativeIndex) {
    const int absoluteNativeIndex = lua_absindex(state, nativeIndex);
    sol::state_view lua(state);
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").push();
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushvalue(state, absoluteNativeIndex);
    lua_rawget(state, ownersIndex);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, ownersIndex);
        return true;
    }
    lua_pop(state, 2);
    void* pointer = nativePointer(state, absoluteNativeIndex);
    if (pointer == nullptr) {
        return false;
    }
    registryTable(lua, protocol::NATIVE_POINTER_OWNERS_REGISTRY_KEY, "v")
        .push();
    const int pointerOwnersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, pointerOwnersIndex);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, pointerOwnersIndex);
        return true;
    }
    lua_pop(state, 2);
    return false;
}

void restoreNativeOwners(lua_State* state) {
    const int resultCount = lua_gettop(state);
    for (int index = 1; index <= resultCount; ++index) {
        const int absoluteIndex = lua_absindex(state, index);
        if (lua_type(state, absoluteIndex) == LUA_TUSERDATA) {
            if (pushNativeOwner(state, absoluteIndex)) {
                lua_replace(state, absoluteIndex);
            }
        } else if (lua_type(state, absoluteIndex) == LUA_TTABLE) {
            const lua_Integer length =
                static_cast<lua_Integer>(lua_rawlen(state, absoluteIndex));
            for (lua_Integer itemIndex = 1; itemIndex <= length; ++itemIndex) {
                lua_rawgeti(state, absoluteIndex, itemIndex);
                const int valueIndex = lua_absindex(state, -1);
                if (lua_type(state, valueIndex) == LUA_TUSERDATA &&
                    pushNativeOwner(state, valueIndex)) {
                    lua_replace(state, valueIndex);
                }
                lua_rawseti(state, absoluteIndex, itemIndex);
            }
        }
    }
}

namespace {

int boundMethodCall(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_pushvalue(state, lua_upvalueindex(2));
    lua_insert(state, 2);
    lua_call(state, argumentCount + 1, LUA_MULTRET);
    restoreNativeOwners(state);
    return lua_gettop(state);
}

int nativeMethodCall(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    if (argumentCount == 0) {
        return luaL_error(state, "Native instance method requires a receiver");
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_pushvalue(state, lua_upvalueindex(2));
    lua_replace(state, 2);
    lua_call(state, argumentCount, LUA_MULTRET);
    restoreNativeOwners(state);
    return lua_gettop(state);
}

}  // namespace

sol::object bindMethod(sol::state_view lua, const sol::object& method,
                       const sol::object& self) {
    lua_State* state = lua.lua_state();
    method.push();
    self.push();
    lua_pushcclosure(state, boundMethodCall, 2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return result;
}

sol::object wrapNativeMethod(sol::state_view lua, const sol::object& method,
                             const sol::object& nativeObject) {
    method.push();
    nativeObject.push();
    lua_pushcclosure(lua.lua_state(), nativeMethodCall, 2);
    sol::object result = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return result;
}

}  // namespace ludork::standard::class_runtime::detail
