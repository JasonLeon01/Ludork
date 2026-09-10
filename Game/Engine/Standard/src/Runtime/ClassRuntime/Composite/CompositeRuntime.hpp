#pragma once

#include "Detail/RuntimeState.hpp"

#include <sol2/sol.hpp>

#include <string>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

sol::object compositeIndexSlow(sol::object target, sol::object key,
                               sol::this_state state);
void invalidateFastIndexEntry(lua_State* state, int cacheIndex);
int compositeIndex(lua_State* state);
int compositeNewIndex(lua_State* state);
sol::table compositeMetatable(sol::state_view lua);
sol::table constructingCompositeMetatable(sol::state_view lua);
void invokeMonitorCallbacks(sol::state_view lua, const sol::table& entry,
                            const sol::object& oldValue,
                            const sol::object& newValue);
void registerMonitor(sol::this_state state, const sol::object& target,
                     const std::string& name,
                     const sol::protected_function& callback,
                     sol::variadic_args options);
void unregisterMonitor(sol::this_state state, const sol::object& target,
                       const std::string& name,
                       sol::optional<std::string> identifier);

}  // namespace ludork::standard::class_runtime::detail
