#pragma once

#include "RuntimeServiceInternals.hpp"

namespace ludork::engine::runtime_detail {

inline constexpr const char* BLUEPRINT_IMPLEMENTATION_CACHE_KEY =
    "Ludork.Engine.blueprintImplementationCache";
inline constexpr const char* BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY =
    "Ludork.Engine.blueprintEventDescriptorCache";
inline constexpr const char* BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY =
    "Ludork.Engine.blueprintCallableParameterCache";

void invokeCompletion(const std::function<void()>& callback);
sol::table classRuntimeEventCache(sol::state_view lua,
                                  const sol::table& classType);
sol::table runtimeEventDescriptor(sol::state_view lua,
                                  const sol::object& method,
                                  const sol::table& classType,
                                  const std::string& eventName);
sol::table runtimeDescriptorParameters(sol::state_view lua,
                                       const sol::table& descriptor);
void invokeNamedRuntimeMethod(sol::state_view lua, const sol::object& object,
                              const sol::object& method,
                              const sol::table& classType,
                              const std::string& eventName,
                              const sol::object& rawKeywordArguments);
bool runtimeMethodHasImplementation(sol::state_view lua,
                                    const sol::object& method);
sol::object blueprintEngineType(sol::state_view lua, const char* name);
bool blueprintIsInstance(sol::this_state state, const sol::object& value,
                         const sol::object& type);
sol::object callRuntimeMethodFirst(
    sol::state_view lua, const sol::object& object, const char* name,
    const std::vector<sol::object>& arguments = {});
bool generatedBlueprintGraphHasExecutableEvent(sol::state_view lua,
                                               const sol::table& classType,
                                               const std::string& eventName);
sol::object generatedBlueprintGraph(sol::state_view lua,
                                    const sol::object& object,
                                    const sol::table& classType);
sol::table blueprintEventKeywordArguments(
    sol::state_view lua, const sol::table& classType,
    const std::string& eventName, const sol::object& rawArguments,
    const sol::object& rawKeywordArguments);
void mergeBlueprintLocalArguments(sol::state_view lua,
                                  const sol::table& classType,
                                  const std::string& eventName,
                                  sol::table keywordArguments,
                                  const sol::object& localGraph);

}  // namespace ludork::engine::runtime_detail
