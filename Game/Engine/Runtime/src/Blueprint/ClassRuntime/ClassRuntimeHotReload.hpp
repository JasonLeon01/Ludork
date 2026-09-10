#pragma once

struct lua_State;

namespace ludork::runtime::class_runtime_detail {

int pushHotReloadMixins(lua_State* state);

void validateHotReloadMixin(lua_State* state, int classIndex,
                            int definitionIndex);

}  // namespace ludork::runtime::class_runtime_detail
