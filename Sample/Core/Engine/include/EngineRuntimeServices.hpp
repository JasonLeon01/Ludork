#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

struct lua_State;

BIND_MODULE_INIT()
LUDORK_ENGINE_API void initializeEngineRuntimeServices(lua_State* state);

LUDORK_ENGINE_API void shutdownEngineRuntimeServices(lua_State* state) noexcept;
