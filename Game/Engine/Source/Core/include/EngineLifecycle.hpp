#pragma once

#include <EngineRuntimeApi.hpp>

struct lua_State;

LUDORK_ENGINE_API void initializeEngineLifecycle(lua_State* state);

namespace ludork::engine {

LUDORK_ENGINE_API void shutdown(lua_State* state) noexcept;

}
