#pragma once

#include <Runtime/Detail/RuntimeServices.hpp>

namespace ludork::engine::runtime_detail {

using ludork::runtime::detail::checkedResult;
using ludork::runtime::detail::classType;
using ludork::runtime::detail::invokeRuntimeFunction;
using ludork::runtime::detail::isClass;
using ludork::runtime::detail::isInstance;
using ludork::runtime::detail::isNativeType;
using ludork::runtime::detail::luaBoolean;
using ludork::runtime::detail::nilObject;
using ludork::runtime::detail::rawEqual;
using ludork::runtime::detail::registryTable;
using ludork::runtime::detail::runtimeAssign;
using ludork::runtime::detail::runtimeClassMro;
using ludork::runtime::detail::runtimeIndex;
using ludork::runtime::detail::runtimeKeys;
using ludork::runtime::detail::runtimeTypeMetadata;

std::function<void()> completionCallback(const sol::object& value);
bool hasBlueprintEvent(sol::this_state state, const sol::object& object,
                       const std::string& eventName);
void dispatchBlueprintEvent(sol::this_state state, const sol::object& object,
                            const sol::object& rawObjectType,
                            const std::string& eventName,
                            const sol::object& rawKeywordArguments,
                            const std::function<void()>& onComplete);
void validateBlueprintEvent(sol::this_state state, const sol::object& object,
                            const std::string& eventName);
bool blueprintGraphHasExecutableEvent(sol::state_view lua,
                                      const sol::object& graph,
                                      const std::string& eventName);
bool blueprintGraphDataHasExecutableEvent(sol::state_view lua,
                                          const sol::object& graphData,
                                          const std::string& eventName);
bool classHasBlueprintEvent(sol::this_state state, const sol::object& rawClass,
                            const std::string& eventName);
bool executeParentBlueprintEvent(sol::this_state state,
                                 const sol::object& object,
                                 const sol::object& rawObjectClass,
                                 const std::string& eventName,
                                 const sol::object& rawParentClass,
                                 const sol::object& rawKeywordArguments,
                                 const sol::object& rawImplementationOwner,
                                 const std::function<void()>& onComplete);
bool executeBlueprintGraph(sol::state_view lua, const sol::object& graph,
                           const std::string& eventName,
                           const sol::object& rawKeywordArguments,
                           const sol::object& graphClass,
                           const std::function<void()>& onComplete);
void clearBlueprintRuntimeCaches(sol::state_view lua);

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
