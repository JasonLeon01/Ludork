#include "Detail/Hierarchy.hpp"
#include <ClassRuntimeProtocol.hpp>
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypedFields.hpp"

#include <LuaError.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <optional>
#include <stdexcept>
#include <string>

namespace ludork::standard::class_runtime::detail {

lua_glue::Object nilObject(lua_glue::StateView lua) {
    return lua_glue::MakeObject(lua, lua_glue::nil);
}

namespace {

int protectedIndexThunk(lua_State* state) {
    lua_pushvalue(state, 2);
    lua_gettable(state, 1);
    return 1;
}

int protectedAssignThunk(lua_State* state) {
    lua_pushvalue(state, 2);
    lua_pushvalue(state, 3);
    lua_settable(state, 1);
    return 0;
}

}  // namespace

std::string popLuaError(lua_State* state, const char* fallback) {
    std::size_t length = 0;
    const char* message = lua_tolstring(state, -1, &length);
    const std::string result = message == nullptr
                                   ? std::string(fallback)
                                   : std::string(message, length);
    lua_pop(state, 1);
    return result;
}

lua_glue::Object protectedIndex(lua_glue::StateView lua,
                                const lua_glue::Object& target,
                                const lua_glue::Object& key) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, protectedIndexThunk);
    target.push(state);
    key.push(state);
    if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
        throw std::runtime_error(popLuaError(state, "Lua indexed read failed"));
    }
    lua_glue::Object result = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return result;
}

void protectedAssign(lua_glue::StateView lua, const lua_glue::Object& target,
                     const lua_glue::Object& key,
                     const lua_glue::Object& value) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, protectedAssignThunk);
    target.push(state);
    key.push(state);
    value.push(state);
    if (ludork::standard::protectedLuaCall(state, 3, 0) != LUA_OK) {
        throw std::runtime_error(
            popLuaError(state, "Lua indexed write failed"));
    }
    clearExplicitNilField(lua, target, key);
}

bool isClass(const lua_glue::Table& value) {
    const lua_glue::Object marker =
        value.raw_get<lua_glue::Object>(protocol::CLASS_MARKER_FIELD);
    return marker.is<bool>() && marker.as<bool>();
}

bool tableHasMetatable(const lua_glue::Table& value) {
    lua_State* state = value.lua_state();
    value.push(state);
    const bool result = lua_getmetatable(state, -1) != 0;
    lua_pop(state, result ? 2 : 1);
    return result;
}

lua_glue::Table createWeakTable(lua_glue::StateView lua, const char* mode) {
    lua_glue::Table result = lua.create_table();
    lua_glue::Table metatable = lua.create_table();
    metatable["__mode"] = mode;
    lua_glue::SetMetatable(result, metatable);
    return result;
}

lua_glue::Table registryTable(lua_glue::StateView lua, const char* key,
                              const char* weakMode) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object value = registry.raw_get<lua_glue::Object>(key);
    if (value.is<lua_glue::Table>()) {
        return value.as<lua_glue::Table>();
    }
    lua_glue::Table result = weakMode == nullptr
                                 ? lua.create_table()
                                 : createWeakTable(lua, weakMode);
    registry.raw_set(key, result);
    return result;
}

lua_glue::Object nativeDeepCopyProtocolsKey(lua_glue::StateView lua) {
    return lua_glue::MakeObject(lua, lua_glue::LightUserdata(static_cast<void*>(
                                         &nativeDeepCopyProtocolsKeyStorage)));
}

lua_glue::Table nativeDeepCopyProtocols(lua_glue::StateView lua) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object key = nativeDeepCopyProtocolsKey(lua);
    const lua_glue::Object existing = registry.raw_get<lua_glue::Object>(key);
    if (existing.get_type() == lua_glue::Type::Table) {
        return existing.as<lua_glue::Table>();
    }
    lua_glue::Table result = lua.create_table();
    registry.raw_set(key, result);
    return result;
}

std::optional<NativeDeepCopyProtocol> findNativeDeepCopyProtocol(
    lua_glue::StateView lua, const lua_glue::Object& nativeType) {
    const lua_glue::Object rawProtocols =
        lua.registry().raw_get<lua_glue::Object>(
            nativeDeepCopyProtocolsKey(lua));
    if (rawProtocols.get_type() != lua_glue::Type::Table) {
        return std::nullopt;
    }
    const lua_glue::Object rawProtocol =
        rawProtocols.as<lua_glue::Table>().raw_get<lua_glue::Object>(
            nativeType);
    if (rawProtocol.get_type() != lua_glue::Type::Userdata) {
        return std::nullopt;
    }
    lua_State* state = lua.lua_state();
    rawProtocol.push(state);
    if (lua_rawlen(state, -1) != sizeof(NativeDeepCopyProtocol)) {
        lua_pop(state, 1);
        return std::nullopt;
    }
    const auto* protocol =
        static_cast<const NativeDeepCopyProtocol*>(lua_touserdata(state, -1));
    const auto result = *protocol;
    lua_pop(state, 1);
    return result;
}

bool tableIsEmpty(const lua_glue::Table& table) {
    for (const auto& entry : table) {
        static_cast<void>(entry);
        return false;
    }
    return true;
}

bool rawBool(const lua_glue::Table& table, const char* name) {
    const lua_glue::Object value = table.raw_get<lua_glue::Object>(name);
    return value.is<bool>() && value.as<bool>();
}

bool luaValuesEqual(lua_glue::StateView lua, const lua_glue::Object& left,
                    const lua_glue::Object& right) {
    lua_State* state = lua.lua_state();
    lua_glue::StackGuard stack(state);
    left.push(state);
    right.push(state);
    return ludork::standard::compareLuaValues(state, -2, -1, LUA_OPEQ);
}

bool objectsRawEqual(const lua_glue::Object& left,
                     const lua_glue::Object& right) {
    return left == right;
}

}  // namespace ludork::standard::class_runtime::detail
