#include "ApplicationRuntime.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

extern "C" {
int luaopen_Engine(lua_State* state);
int luaopen_GlobalCore(lua_State* state);
int luaopen_GlobalFunctions(lua_State* state);
}

namespace {

void registerPreloadedModule(lua_State* state, const char* name,
                             lua_CFunction openFunction) {
    lua_pushcfunction(state, openFunction);
    lua_setfield(state, -2, name);
}

}  // namespace

namespace ludork::application::detail {

void registerRuntimeModules(lua_State* state) {
    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    registerPreloadedModule(state, "Engine", luaopen_Engine);
    registerPreloadedModule(state, "GlobalCore", luaopen_GlobalCore);
    registerPreloadedModule(state, "GlobalFunctions", luaopen_GlobalFunctions);
    lua_pop(state, 1);
}

}  // namespace ludork::application::detail
