#include "Detail/ClassNativeInterop.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>

namespace ludork::standard::class_native {

namespace {

constexpr const char* NEXT_INSTANCE_ID_KEY = "Ludork.Class.nextInstanceId";

}

std::size_t nextInstanceId(lua_glue::StateView lua) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object current =
        registry.raw_get<lua_glue::Object>(NEXT_INSTANCE_ID_KEY);
    const std::size_t result =
        current.is<std::size_t>() ? current.as<std::size_t>() : 1;
    registry.raw_set(NEXT_INSTANCE_ID_KEY, result + 1);
    return result;
}

lua_glue::Table getUserFields(lua_glue::StateView lua,
                              const lua_glue::Object& value, bool create) {
    lua_State* state = lua.lua_state();
    value.push(lua.lua_state());
    const int valueIndex = lua_gettop(state);
    const int fieldType = lua_getiuservalue(state, valueIndex, 1);
    if (fieldType == LUA_TTABLE) {
        lua_glue::Table fields = lua_glue::Read<lua_glue::Table>(state, -1);
        lua_pop(state, 2);
        return fields;
    }
    lua_pop(state, 1);
    if (!create) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    lua_glue::Table fields = lua.create_table();
    fields.push(lua.lua_state());
    lua_setiuservalue(state, valueIndex, 1);
    lua_pop(state, 1);
    return fields;
}

lua_glue::Table getObjectMetatable(lua_glue::StateView lua,
                                   const lua_glue::Object& value) {
    lua_State* state = lua.lua_state();
    value.push(lua.lua_state());
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    lua_glue::Table result = lua_glue::Read<lua_glue::Table>(state, -1);
    lua_pop(state, 2);
    return result;
}

}  // namespace ludork::standard::class_native
