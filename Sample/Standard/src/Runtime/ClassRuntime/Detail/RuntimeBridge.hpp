#pragma once

#include <sol2/sol.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

inline constexpr const char* RUNTIME_SERVICES_KEY =
    "Ludork.Class.runtimeServices";

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context);
int invokeRuntimeFunction(sol::state_view lua, const sol::object& rawCallable,
                          const std::vector<sol::object>& arguments,
                          const char* context);
sol::table resolverMro(sol::state_view lua, const sol::table& classTable);
void registerRuntimeService(sol::this_state state, const std::string& name,
                            const sol::protected_function& callback);
void unregisterRuntimeService(sol::this_state state, const std::string& name);
int runtimeClassResolver(lua_State* state);

}  // namespace ludork::standard::class_runtime::detail
