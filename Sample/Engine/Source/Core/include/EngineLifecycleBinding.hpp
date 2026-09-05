#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

#include <EngineLifecycle.hpp>

BIND_MODULE_INIT()
LUDORK_ENGINE_API void initializeEngineLifecycle(lua_State* state);
