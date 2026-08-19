#pragma once

#include "Detail/CopyRuntime.hpp"

#include <sol2/forward.hpp>

struct lua_State;

namespace ludork::standard::class_runtime {

sol::table createModule(sol::state_view lua);
void shutdown(lua_State* state) noexcept;

}  // namespace ludork::standard::class_runtime
