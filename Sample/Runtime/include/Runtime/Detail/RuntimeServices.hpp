#pragma once

#include <RuntimeApi.hpp>

#include <sol2/sol.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ludork::runtime::detail {

LUDORK_RUNTIME_API void ensureRuntimeLuaStack(lua_State* state,
                                              std::size_t count,
                                              const char* context);
LUDORK_RUNTIME_API int invokeRuntimeFunction(
    lua_State* state, const sol::object& callable,
    const std::vector<sol::object>& arguments, const char* context);
LUDORK_RUNTIME_API sol::object nilObject(sol::state_view lua);
LUDORK_RUNTIME_API sol::table registryTable(sol::state_view lua,
                                            const char* key,
                                            const char* weakMode = nullptr);
LUDORK_RUNTIME_API sol::table objectMetatable(sol::state_view lua,
                                              const sol::object& value);
LUDORK_RUNTIME_API sol::object classType(sol::this_state state,
                                         const sol::object& value);
LUDORK_RUNTIME_API sol::object findRuntimeClassModule(
    sol::state_view lua, const sol::object& classReference);
LUDORK_RUNTIME_API sol::object syntheticRuntimeMetadata(
    sol::state_view lua, const sol::table& classTable);
LUDORK_RUNTIME_API sol::object runtimeTypeMetadata(sol::state_view lua,
                                                   const sol::table& classType);
LUDORK_RUNTIME_API sol::table collectRuntimeAttrMetadata(
    sol::state_view lua, const sol::table& owner);
LUDORK_RUNTIME_API std::pair<sol::object, sol::object> resolveRuntimeConfigVar(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName);
LUDORK_RUNTIME_API std::pair<sol::object, sol::object>
resolveRuntimeMemberMetadata(sol::state_view lua, const sol::object& owner,
                             const sol::object& rawName);
LUDORK_RUNTIME_API sol::object evaluateRuntimeExpression(
    sol::state_view lua, const sol::object& rawExpression,
    const sol::object& rawEnvironment);
LUDORK_RUNTIME_API sol::object resolveRuntimeMetadataType(
    sol::state_view lua, const sol::object& typeReference,
    const sol::object& declaringModule);
LUDORK_RUNTIME_API bool runtimeSequence(const sol::table& table,
                                        std::vector<sol::object>& values);
LUDORK_RUNTIME_API sol::object runtimeIndex(sol::state_view lua,
                                            const sol::object& target,
                                            const sol::object& key, bool raw);
LUDORK_RUNTIME_API bool rawBool(const sol::table& table, const char* name);
LUDORK_RUNTIME_API bool isClass(const sol::table& value);
LUDORK_RUNTIME_API bool isNativeType(sol::state_view lua,
                                     const sol::table& value);
LUDORK_RUNTIME_API bool isInstance(sol::this_state state,
                                   const sol::object& value,
                                   const sol::table& targetClass);
LUDORK_RUNTIME_API bool rawEqual(sol::state_view lua, const sol::object& left,
                                 const sol::object& right);
LUDORK_RUNTIME_API sol::object checkedResult(
    sol::state_view lua, sol::protected_function_result& result);
LUDORK_RUNTIME_API sol::table requireLuaTable(sol::state_view lua,
                                              const char* moduleName);
LUDORK_RUNTIME_API sol::object requireRuntimeType(sol::state_view lua,
                                                  const std::string& modulePath,
                                                  const std::string& typeName);
LUDORK_RUNTIME_API bool luaBoolean(const sol::object& value);
LUDORK_RUNTIME_API std::vector<sol::table> runtimeClassMro(
    sol::state_view lua, const sol::table& classTable);
LUDORK_RUNTIME_API void runtimeAssign(sol::state_view lua,
                                      const sol::object& target,
                                      const sol::object& key,
                                      const sol::object& value, bool raw);
LUDORK_RUNTIME_API std::vector<sol::object> runtimeKeys(
    sol::state_view lua, const sol::object& target, bool raw);

LUDORK_RUNTIME_API void clearRuntimeCaches(sol::state_view lua);

}  // namespace ludork::runtime::detail
