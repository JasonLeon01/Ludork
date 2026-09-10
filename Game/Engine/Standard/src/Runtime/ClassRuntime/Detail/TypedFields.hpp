#pragma once

#include <sol2/forward.hpp>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

bool hasExplicitNilField(lua_State* state, int targetIndex, int keyIndex);
void clearExplicitNilField(lua_State* state, int targetIndex, int keyIndex);
bool hasExplicitNilField(sol::state_view lua, const sol::object& target,
                         const sol::object& key);
void clearExplicitNilField(sol::state_view lua, const sol::object& target,
                           const sol::object& key);
void markExplicitNilField(sol::state_view lua, const sol::object& target,
                          const sol::object& key);
sol::table explicitNilFieldKeys(sol::state_view lua, const sol::object& target);
void copyExplicitNilFields(sol::state_view lua, const sol::object& source,
                           const sol::object& target);
void clearExplicitNilFields(sol::state_view lua, const sol::object& target);
void clearExplicitNilFields(lua_State* state);

}  // namespace ludork::standard::class_runtime::detail
