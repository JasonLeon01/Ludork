#pragma once

#include <StandardApi.hpp>

struct lua_State;

namespace ludork::standard {

LUDORK_STANDARD_API void initialize(lua_State* state);
LUDORK_STANDARD_API void update(lua_State* state);
LUDORK_STANDARD_API void shutdown(lua_State* state) noexcept;

}  // namespace ludork::standard
