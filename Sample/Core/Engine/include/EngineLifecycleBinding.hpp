#pragma once

#include <EngineRuntimeApi.hpp>

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineLifecycle.hpp>

BIND_MODULE_INIT()
LUDORK_ENGINE_API void initializeEngineLifecycle(lua_State* state);
