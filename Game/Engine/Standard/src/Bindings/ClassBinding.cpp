#include "Bindings.hpp"

#include "Runtime/ClassRuntime/ClassRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace ludork::standard::binding {

void registerClass(lua_glue::StateView lua) {
    lua_glue::Table module = ludork::standard::class_runtime::createModule(lua);
    module.push(lua.lua_state());
    lua_setglobal(lua.lua_state(), "Class");

    luaL_getsubtable(lua.lua_state(), LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    module.push(lua.lua_state());
    lua_setfield(lua.lua_state(), -2, "Class");
    lua_pop(lua.lua_state(), 1);
}

}  // namespace ludork::standard::binding
