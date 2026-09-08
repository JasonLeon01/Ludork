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
void invokeMonitorCallback(sol::state_view lua, sol::table entry,
                           const sol::object& oldValue,
                           const sol::object& newValue);
void registerMonitor(sol::this_state state, const sol::object& target,
                     const std::string& name,
                     const sol::protected_function& callback,
                     sol::optional<sol::table> params,
                     sol::optional<bool> notifyEqualWrites);
void unregisterMonitor(sol::this_state state, const sol::object& target,
                       const std::string& name);

}  // namespace ludork::standard::class_runtime::detail
