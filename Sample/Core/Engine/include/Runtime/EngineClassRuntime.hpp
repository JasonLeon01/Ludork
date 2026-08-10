#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>

struct lua_State;

BIND_MODULE_INIT()
LUDORK_ENGINE_API void initializeEngineClassRuntime(lua_State* state);

LUDORK_ENGINE_API void shutdownEngineClassRuntime(lua_State* state) noexcept;
