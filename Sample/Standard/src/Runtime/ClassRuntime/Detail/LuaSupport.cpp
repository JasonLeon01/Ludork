#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"

#include <LuaError.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <optional>
#include <stdexcept>
#include <string>

namespace ludork::standard::class_runtime::detail {

sol::object nilObject(sol::state_view lua) {
    return sol::make_object(lua, sol::lua_nil);
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

sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, protectedIndexThunk);
    target.push();
    key.push();
    if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
        throw std::runtime_error(popLuaError(state, "Lua indexed read failed"));
    }
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return result;
}

void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value) {
    lua_State* state = lua.lua_state();
    lua_pushcfunction(state, protectedAssignThunk);
    target.push();
    key.push();
    value.push();
    if (ludork::standard::protectedLuaCall(state, 3, 0) != LUA_OK) {
        throw std::runtime_error(
            popLuaError(state, "Lua indexed write failed"));
    }
}

bool isClass(const sol::table& value) {
    const sol::object marker = value.raw_get<sol::object>("__ludorkClass");
    return marker.is<bool>() && marker.as<bool>();
}

bool tableHasMetatable(const sol::table& value) {
    lua_State* state = value.lua_state();
    value.push();
    const bool result = lua_getmetatable(state, -1) != 0;
    lua_pop(state, result ? 2 : 1);
    return result;
}

sol::table createWeakTable(sol::state_view lua, const char* mode) {
    sol::table result = lua.create_table();
    sol::table metatable = lua.create_table();
    metatable["__mode"] = mode;
    result[sol::metatable_key] = metatable;
    return result;
}

sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode) {
    sol::table registry = lua.registry();
    const sol::object value = registry.raw_get<sol::object>(key);
    if (value.is<sol::table>()) {
        return value.as<sol::table>();
    }
    sol::table result = weakMode == nullptr ? lua.create_table()
                                            : createWeakTable(lua, weakMode);
    registry.raw_set(key, result);
    return result;
}

sol::object nativeDeepCopyProtocolsKey(sol::state_view lua) {
    return sol::make_object(lua, sol::lightuserdata_value(static_cast<void*>(
                                     &nativeDeepCopyProtocolsKeyStorage)));
}

sol::table nativeDeepCopyProtocols(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object key = nativeDeepCopyProtocolsKey(lua);
    const sol::object existing = registry.raw_get<sol::object>(key);
    if (existing.get_type() == sol::type::table) {
        return existing.as<sol::table>();
    }
    sol::table result = lua.create_table();
    registry.raw_set(key, result);
    return result;
}

std::optional<NativeDeepCopyProtocol> findNativeDeepCopyProtocol(
    sol::state_view lua, const sol::object& nativeType) {
    const sol::object rawProtocols =
        lua.registry().raw_get<sol::object>(nativeDeepCopyProtocolsKey(lua));
    if (rawProtocols.get_type() != sol::type::table) {
        return std::nullopt;
    }
    const sol::object rawProtocol =
        rawProtocols.as<sol::table>().raw_get<sol::object>(nativeType);
    if (rawProtocol.get_type() != sol::type::userdata) {
        return std::nullopt;
    }
    lua_State* state = lua.lua_state();
    rawProtocol.push();
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

bool tableIsEmpty(const sol::table& table) {
    for (const auto& entry : table) {
        static_cast<void>(entry);
        return false;
    }
    return true;
}

bool rawBool(const sol::table& table, const char* name) {
    const sol::object value = table.raw_get<sol::object>(name);
    return value.is<bool>() && value.as<bool>();
}

bool luaValuesEqual(sol::state_view lua, const sol::object& left,
                    const sol::object& right) {
    left.push();
    right.push();
    const bool equal = lua_compare(lua.lua_state(), -2, -1, LUA_OPEQ) != 0;
    lua_pop(lua.lua_state(), 2);
    return equal;
}

bool objectsRawEqual(const sol::object& left, const sol::object& right) {
    lua_State* state = left.lua_state();
    left.push();
    right.push();
    const bool result = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    return result;
}

}  // namespace ludork::standard::class_runtime::detail
