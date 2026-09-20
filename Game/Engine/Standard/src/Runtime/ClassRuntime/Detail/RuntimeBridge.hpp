#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context);
int invokeRuntimeFunction(lua_glue::StateView lua,
                          const lua_glue::Object& rawCallable,
                          const std::vector<lua_glue::Object>& arguments,
                          const char* context);
lua_glue::Table resolverMro(lua_glue::StateView lua,
                            const lua_glue::Table& classTable);

}  // namespace ludork::standard::class_runtime::detail
