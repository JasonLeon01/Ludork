#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <sol2/sol.hpp>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace ludork::engine::class_runtime_detail {

inline constexpr const char* CLASS_RESOLVER_STATE_KEY =
    "Ludork.Engine.classResolverState";

sol::object nilObject(sol::state_view lua);
sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result,
                          int index = 0);
sol::table requireTable(sol::state_view lua, const std::string& moduleName);
sol::table resolverState(sol::state_view lua);
sol::object callRuntimeServiceFirst(sol::state_view lua,
                                    const std::string& name,
                                    const std::vector<sol::object>& arguments);
sol::object compileGraphTemplate(sol::state_view lua, const sol::table& data,
                                 const sol::object& classType);
bool classGraphHasExecutableEvent(sol::state_view lua,
                                  const std::string& classPath,
                                  const std::string& eventName);
sol::object instantiateClassGraph(sol::state_view lua,
                                  const std::string& classPath,
                                  const sol::object& parent);
RuntimeValue runtimeValue(const sol::object& value);
sol::object luaValue(sol::state_view lua, const RuntimeValue& value);
std::string declaringModule(const sol::object& value);
sol::object cloneMetadataValue(sol::state_view lua, const sol::object& value,
                               const sol::table& fieldMetadata,
                               const std::string& fallbackModule = {});
sol::object cloneAttrValue(sol::state_view lua, const sol::table& parentClass,
                           const sol::object& key, const sol::object& value,
                           const sol::object& rawMetadata,
                           const sol::object& rawTargetType);
sol::table configReferences(sol::state_view lua, const sol::table& owner);
bool moduleExists(sol::state_view lua, const std::string& moduleName);
std::string normalizeScriptMixinPath(const std::string& value);
sol::table loadScriptMixin(sol::state_view lua, const std::string& classPath,
                           const std::string& scriptPath);
void mergeScriptMixin(sol::state_view lua, const sol::table& parentClass,
                      const sol::table& mixin, sol::table definition,
                      sol::table instanceAttrs, const std::string& classPath,
                      const std::string& scriptPath);
void applyConfigValues(sol::state_view lua, const sol::table& parentClass,
                       sol::table classAttrs, const sol::table& references);
void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const sol::object& self,
                                 const sol::variadic_args& arguments);
std::tuple<sol::object, sol::object> resolveClass(sol::state_view lua,
                                                  const sol::object& rawPath,
                                                  const sol::object& rawRoot);

}  // namespace ludork::engine::class_runtime_detail
