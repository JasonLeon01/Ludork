#pragma once

#include "RuntimeSubsystemServices.hpp"

namespace ludork::engine::runtime_detail {

bool rawBool(const sol::table& table, const char* name);
bool isClass(const sol::table& value);
bool isNativeType(sol::state_view lua, const sol::table& value);
bool isInstance(sol::this_state state, const sol::object& value,
                const sol::table& targetClass);
bool rawEqual(sol::state_view lua, const sol::object& left,
              const sol::object& right);
sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result);
sol::table requireLuaTable(sol::state_view lua, const char* moduleName);
sol::object requireRuntimeType(sol::state_view lua,
                               const std::string& modulePath,
                               const std::string& typeName);
bool luaBoolean(const sol::object& value);
sol::object callRegisteredRuntimeServiceFirst(
    sol::state_view lua, const std::string& name,
    const std::vector<sol::object>& arguments = {});
std::vector<sol::table> runtimeClassMro(sol::state_view lua,
                                        const sol::table& classTable);
void runtimeAssign(sol::state_view lua, const sol::object& target,
                   const sol::object& key, const sol::object& value, bool raw);
std::vector<sol::object> runtimeKeys(sol::state_view lua,
                                     const sol::object& target, bool raw);
sol::table runtimeStringKeys(sol::state_view lua,
                             const std::vector<sol::object>& keys);

void clearRuntimeCommonCaches(sol::state_view lua);
void clearBlueprintRuntimeCaches(sol::state_view lua);

}  // namespace ludork::engine::runtime_detail
