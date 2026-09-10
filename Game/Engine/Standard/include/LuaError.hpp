#pragma once

#include <StandardApi.hpp>

#include <string>

struct lua_State;

namespace ludork::standard {

LUDORK_STANDARD_API void installLuaErrorHandler(lua_State* state);
LUDORK_STANDARD_API int protectedLuaCall(lua_State* state, int argumentCount,
                                         int resultCount);
LUDORK_STANDARD_API std::string luaErrorMessage(lua_State* state, int index);

}  // namespace ludork::standard
