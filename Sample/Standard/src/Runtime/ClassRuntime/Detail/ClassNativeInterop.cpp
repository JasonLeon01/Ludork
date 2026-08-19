#include "Detail/ClassNativeInterop.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>

namespace ludork::standard::class_native {

namespace {

constexpr const char* NEXT_INSTANCE_ID_KEY = "Ludork.Class.nextInstanceId";

}

std::size_t nextInstanceId(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object current =
        registry.raw_get<sol::object>(NEXT_INSTANCE_ID_KEY);
    const std::size_t result =
        current.is<std::size_t>() ? current.as<std::size_t>() : 1;
    registry.raw_set(NEXT_INSTANCE_ID_KEY, result + 1);
    return result;
}

sol::table getUserFields(sol::state_view lua, const sol::object& value,
                         bool create) {
    lua_State* state = lua.lua_state();
    value.push();
    const int valueIndex = lua_gettop(state);
    const int fieldType = lua_getiuservalue(state, valueIndex, 1);
    if (fieldType == LUA_TTABLE) {
        sol::table fields = sol::stack::get<sol::table>(state, -1);
        lua_pop(state, 2);
        return fields;
    }
    lua_pop(state, 1);
    if (!create) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    sol::table fields = lua.create_table();
    fields.push();
    lua_setiuservalue(state, valueIndex, 1);
    lua_pop(state, 1);
    return fields;
}

sol::table getObjectMetatable(sol::state_view lua, const sol::object& value) {
    lua_State* state = lua.lua_state();
    value.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    sol::table result = sol::stack::get<sol::table>(state, -1);
    lua_pop(state, 2);
    return result;
}

}  // namespace ludork::standard::class_native
