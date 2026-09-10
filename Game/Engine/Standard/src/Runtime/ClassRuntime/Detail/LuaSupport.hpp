#pragma once

#include "Detail/CopyRuntime.hpp"

#include <sol2/sol.hpp>

#include <optional>
#include <string>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

sol::object nilObject(sol::state_view lua);
std::string popLuaError(lua_State* state, const char* fallback);
sol::object protectedIndex(sol::state_view lua, const sol::object& target,
                           const sol::object& key);
void protectedAssign(sol::state_view lua, const sol::object& target,
                     const sol::object& key, const sol::object& value);
bool tableHasMetatable(const sol::table& value);
sol::table createWeakTable(sol::state_view lua, const char* mode);
sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode = nullptr);
sol::object nativeDeepCopyProtocolsKey(sol::state_view lua);
sol::table nativeDeepCopyProtocols(sol::state_view lua);
std::optional<NativeDeepCopyProtocol> findNativeDeepCopyProtocol(
    sol::state_view lua, const sol::object& nativeType);
bool tableIsEmpty(const sol::table& table);
bool rawBool(const sol::table& table, const char* name);
bool luaValuesEqual(sol::state_view lua, const sol::object& left,
                    const sol::object& right);
bool objectsRawEqual(const sol::object& left, const sol::object& right);

}  // namespace ludork::standard::class_runtime::detail
