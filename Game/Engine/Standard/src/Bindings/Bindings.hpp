#pragma once

#include <LuaGlue/LuaGlue.hpp>

struct lua_State;

namespace ludork::standard::binding {

void registerClass(lua_glue::StateView lua);
void registerContainers(lua_glue::StateView lua);
void registerConfigParser(lua_glue::StateView lua);
void registerCodecs(lua_glue::StateView lua);
void registerSystemServices(lua_glue::StateView lua);
void registerAsyncio(lua_glue::StateView lua);
void registerFileBatch(lua_glue::StateView lua);
void registerString(lua_glue::StateView lua);
void registerTable(lua_glue::StateView lua);
void updateAsyncio(lua_glue::StateView lua);
void shutdownAsyncio(lua_glue::StateView lua) noexcept;
void shutdownFileBatch(lua_glue::StateView lua) noexcept;
void clearFileBatchJsonRuntime(lua_State* state) noexcept;
void shutdownContainers(lua_State* state) noexcept;

}  // namespace ludork::standard::binding
