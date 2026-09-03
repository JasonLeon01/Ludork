#pragma once

#include <sol2/forward.hpp>

struct lua_State;

namespace ludork::standard::binding {

void registerClass(sol::state_view lua);
void registerContainers(sol::state_view lua);
void registerConfigParser(sol::state_view lua);
void registerCodecs(sol::state_view lua);
void registerSystemServices(sol::state_view lua);
void registerAsyncio(sol::state_view lua);
void registerFileBatch(sol::state_view lua);
void registerString(sol::state_view lua);
void registerTable(sol::state_view lua);
void updateAsyncio(sol::state_view lua);
void shutdownAsyncio(sol::state_view lua) noexcept;
void shutdownFileBatch(sol::state_view lua) noexcept;
void clearFileBatchJsonRuntime(lua_State* state) noexcept;
void shutdownContainers(lua_State* state) noexcept;

}  // namespace ludork::standard::binding
