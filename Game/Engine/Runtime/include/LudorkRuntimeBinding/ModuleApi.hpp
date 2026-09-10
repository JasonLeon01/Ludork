#pragma once

struct lua_State;

#if defined(_WIN32)
#define LUDORK_LUA_API extern "C" __declspec(dllexport)
#else
#define LUDORK_LUA_API extern "C" __attribute__((visibility("default")))
#endif
