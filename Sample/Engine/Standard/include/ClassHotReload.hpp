#pragma once

#include <StandardApi.hpp>

struct lua_State;

namespace ludork::standard::class_runtime {

LUDORK_STANDARD_API bool isHotReloadClass(lua_State* state, int index);

LUDORK_STANDARD_API bool isHotReloadProtectedTable(lua_State* state, int index);

LUDORK_STANDARD_API void validateHotReloadClass(lua_State* state, int oldIndex,
                                                int newIndex,
                                                int candidateToLiveTableIndex);

LUDORK_STANDARD_API void commitHotReloadClass(lua_State* state, int oldIndex,
                                              int newIndex);

}  // namespace ludork::standard::class_runtime
