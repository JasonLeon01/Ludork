#include <LuaError.hpp>
#include "Native/NativeRuntime.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>

namespace ludork::standard::class_runtime::detail {

bool nativeTypeAccepts(lua_glue::StateView lua,
                       const lua_glue::Table& nativeType,
                       const lua_glue::Object& value) {
    const lua_glue::Object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<lua_glue::Table>()) {
        return false;
    }
    const lua_glue::Object rawIs =
        rawTypeInfo.as<lua_glue::Table>().raw_get<lua_glue::Object>("is");
    if (!rawIs.is<lua_glue::Function>()) {
        return false;
    }
    lua_glue::CallResult result = rawIs.as<lua_glue::Function>()(value);
    return result.valid() && result.get_type() == lua_glue::Type::Boolean &&
           result.get<bool>();
}

void registerMethodOwner(lua_glue::StateView lua,
                         const lua_glue::Table& classTable,
                         const lua_glue::Object& value) {
    if (!value.is<lua_glue::Function>()) {
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
    lua_getfield(state, -1, protocol::CLASS_TYPE_FIELD);
    const bool native = lua_istable(state, -1);
    lua_pop(state, 2);
    if (composite || !native) {
        return nullptr;
    }
    return lua_glue::NativePointer(state, absoluteIndex);
}

bool pushCompositeNatives(lua_State* state, int index) {
    const int absoluteIndex = lua_absindex(state, index);
    if (lua_type(state, absoluteIndex) != LUA_TUSERDATA ||
        lua_getmetatable(state, absoluteIndex) == 0) {
        return false;
    }
    lua_getfield(state, -1, protocol::COMPOSITE_MARKER_FIELD);
    const bool composite = lua_toboolean(state, -1) != 0;
    lua_pop(state, 2);
    if (!composite) {
        return false;
    }
    if (lua_getiuservalue(state, absoluteIndex, 1) != LUA_TTABLE) {
        lua_pop(state, 1);
        return false;
    }
    lua_getfield(state, -1, protocol::NATIVE_OBJECTS_FIELD);
    lua_remove(state, -2);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        return false;
    }
    return true;
}

void* resolveCompositePointer(lua_State* state, int index,
                              std::string_view typeKey) {
    lua_glue::StackGuard stack(state);
    if (!pushCompositeNatives(state, index)) {
        return nullptr;
    }
    const int natives = lua_absindex(state, -1);
    lua_pushnil(state);
    while (lua_next(state, natives) != 0) {
        if (lua_glue::NativeTypeTable(state, -1).valid()) {
            if (void* pointer = lua_glue::NativePointer(state, -1, typeKey)) {
                return pointer;
            }
        }
        lua_pop(state, 1);
    }
    return nullptr;
}

std::shared_ptr<void> resolveCompositeOwner(lua_State* state, int index,
                                            std::string_view typeKey) {
    lua_glue::StackGuard stack(state);
    if (!pushCompositeNatives(state, index)) {
        return {};
    }
    const int natives = lua_absindex(state, -1);
    lua_pushnil(state);
    while (lua_next(state, natives) != 0) {
        if (lua_glue::NativeTypeTable(state, -1).valid() &&
            lua_glue::NativePointer(state, -1, typeKey) != nullptr) {
            return lua_glue::NativeSharedOwner(state, -1, typeKey);
        }
        lua_pop(state, 1);
    }
    return {};
}

int boundMethodCall(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        const int argumentCount = lua_gettop(state);
        lua_pushvalue(state, lua_upvalueindex(1));
        lua_insert(state, 1);
        lua_pushvalue(state, lua_upvalueindex(2));
        lua_insert(state, 2);
        if (ludork::standard::protectedLuaCall(state, argumentCount + 1,
                                               LUA_MULTRET) != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        restoreNativeOwners(state);
        return lua_gettop(state);
    });
}

int nativeMethodCall(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        const int argumentCount = lua_gettop(state);
        if (argumentCount == 0) {
            throw std::invalid_argument(
                "Native instance method requires a receiver");
        }
        lua_pushvalue(state, lua_upvalueindex(1));
        lua_insert(state, 1);
        lua_pushvalue(state, lua_upvalueindex(2));
        lua_replace(state, 2);
        if (ludork::standard::protectedLuaCall(state, argumentCount,
                                               LUA_MULTRET) != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        restoreNativeOwners(state);
        return lua_gettop(state);
    });
}

}  // namespace

void registerNativeInterop(lua_State* state) {
    lua_glue::RegisterExternalResolver(state, &resolveCompositePointer,
                                       &resolveCompositeOwner);
}

void registerNativePointerOwner(lua_glue::StateView lua,
                                const lua_glue::Object& nativeObject,
                                const lua_glue::Object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push(lua.lua_state());
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    registryTable(lua, protocol::NATIVE_POINTER_OWNERS_REGISTRY_KEY, "v")
        .push(lua.lua_state());
    lua_pushlightuserdata(state, pointer);
    owner.push(lua.lua_state());
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

void unregisterNativePointerOwner(lua_glue::StateView lua,
                                  const lua_glue::Object& nativeObject,
                                  const lua_glue::Object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push(lua.lua_state());
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    lua_glue::Table owners =
        registryTable(lua, protocol::NATIVE_POINTER_OWNERS_REGISTRY_KEY, "v");
    owners.push(lua.lua_state());
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, ownersIndex);
    owner.push(lua.lua_state());
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
    lua_glue::StateView lua(state);
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").push(lua.lua_state());
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
        .push(lua.lua_state());
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

lua_glue::Object bindMethod(lua_glue::StateView lua,
                            const lua_glue::Object& method,
                            const lua_glue::Object& self) {
    lua_State* state = lua.lua_state();
    method.push(lua.lua_state());
    self.push(lua.lua_state());
    lua_pushcclosure(state, boundMethodCall, 2);
    lua_glue::Object result = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return result;
}

lua_glue::Object wrapNativeMethod(lua_glue::StateView lua,
                                  const lua_glue::Object& method,
                                  const lua_glue::Object& nativeObject) {
    method.push(lua.lua_state());
    nativeObject.push(lua.lua_state());
    lua_pushcclosure(lua.lua_state(), nativeMethodCall, 2);
    lua_glue::Object result =
        lua_glue::Read<lua_glue::Object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return result;
}

}  // namespace ludork::standard::class_runtime::detail
