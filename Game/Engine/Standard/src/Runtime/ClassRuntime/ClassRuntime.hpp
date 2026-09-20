#pragma once

#include "Detail/CopyRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

struct lua_State;

namespace ludork::standard::class_runtime {

lua_glue::Table createModule(lua_glue::StateView lua);
void shutdown(lua_State* state) noexcept;

}  // namespace ludork::standard::class_runtime
