#pragma once

#include <sol2/sol.hpp>

#include <cstddef>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context);
int invokeRuntimeFunction(sol::state_view lua, const sol::object& rawCallable,
                          const std::vector<sol::object>& arguments,
                          const char* context);
sol::table resolverMro(sol::state_view lua, const sol::table& classTable);

}  // namespace ludork::standard::class_runtime::detail
