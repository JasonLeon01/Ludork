#pragma once

#include <StandardApi.hpp>

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <vector>

struct lua_State;

namespace ludork::standard {

LUDORK_STANDARD_API void ensureLuaStack(lua_State* state, std::size_t count,
                                        const char* context);

LUDORK_STANDARD_API int invokeLuaFunction(
    lua_State* state, const lua_glue::Object& callable,
    const std::vector<lua_glue::Object>& arguments, const char* context);

}  // namespace ludork::standard
