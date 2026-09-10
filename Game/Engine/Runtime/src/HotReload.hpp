#pragma once

struct lua_State;

namespace ludork::runtime {

void initializeHotReload(lua_State* state);
void shutdownHotReload(lua_State* state) noexcept;

}  // namespace ludork::runtime
