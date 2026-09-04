#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_MODULE_INIT()
LUDORK_GLOBAL_API void initializeGlobalLifecycle(lua_State* state);
