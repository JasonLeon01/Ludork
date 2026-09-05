#pragma once

#include <Runtime/RuntimeReference.hpp>

class Graph;

namespace ludork::engine::runtime_detail {

std::shared_ptr<Graph> requireBlueprintGraph(const RuntimeValue& graph);

using namespace ludork::runtime::reference;

std::function<void()> completionCallback(const RuntimeValue& value);
bool hasBlueprintEvent(const RuntimeValue& object,
                       const std::string& eventName);
void dispatchBlueprintEvent(const RuntimeValue& object,
                            const RuntimeValue& rawObjectType,
                            const std::string& eventName,
                            const RuntimeValue& rawKeywordArguments,
                            const std::function<void()>& onComplete);
void validateBlueprintEvent(const RuntimeValue& object,
                            const std::string& eventName);
bool blueprintGraphHasExecutableEvent(const RuntimeValue& graph,
                                      const std::string& eventName);
bool blueprintGraphDataHasExecutableEvent(const RuntimeValue& graphData,
                                          const std::string& eventName);
bool classHasBlueprintEvent(const RuntimeValue& rawClass,
                            const std::string& eventName);
bool executeParentBlueprintEvent(const RuntimeValue& object,
                                 const RuntimeValue& rawObjectClass,
                                 const std::string& eventName,
                                 const RuntimeValue& rawParentClass,
                                 const RuntimeValue& rawKeywordArguments,
                                 const RuntimeValue& rawImplementationOwner,
                                 const std::function<void()>& onComplete);
bool executeBlueprintGraph(const RuntimeValue& graph,
                           const std::string& eventName,
                           const RuntimeValue& rawKeywordArguments,
                           const RuntimeValue& graphClass,
                           const std::function<void()>& onComplete);
void clearBlueprintRuntimeCaches();

inline constexpr const char* BLUEPRINT_IMPLEMENTATION_CACHE_KEY =
    "Ludork.Engine.blueprintImplementationCache";
inline constexpr const char* BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY =
    "Ludork.Engine.blueprintEventDescriptorCache";
inline constexpr const char* BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY =
    "Ludork.Engine.blueprintCallableParameterCache";

void invokeCompletion(const std::function<void()>& callback);
RuntimeValue classRuntimeEventCache(const RuntimeValue& classType);
RuntimeValue runtimeEventDescriptor(const RuntimeValue& method,
                                    const RuntimeValue& classType,
                                    const std::string& eventName);
RuntimeValue runtimeDescriptorParameters(const RuntimeValue& descriptor);
void invokeNamedRuntimeMethod(const RuntimeValue& object,
                              const RuntimeValue& method,
                              const RuntimeValue& classType,
                              const std::string& eventName,
                              const RuntimeValue& rawKeywordArguments);
bool runtimeMethodHasImplementation(const RuntimeValue& method);
RuntimeValue blueprintEngineType(const char* name);
bool blueprintIsInstance(const RuntimeValue& value, const RuntimeValue& type);
RuntimeValue callRuntimeMethodFirst(
    const RuntimeValue& object, const char* name,
    const std::vector<RuntimeValue>& arguments = {});
bool generatedBlueprintGraphHasExecutableEvent(const RuntimeValue& classType,
                                               const std::string& eventName);
RuntimeValue generatedBlueprintGraph(const RuntimeValue& object,
                                     const RuntimeValue& classType);
RuntimeValue blueprintEventKeywordArguments(
    const RuntimeValue& classType, const std::string& eventName,
    const RuntimeValue& rawArguments, const RuntimeValue& rawKeywordArguments);
void mergeBlueprintLocalArguments(const RuntimeValue& classType,
                                  const std::string& eventName,
                                  RuntimeValue keywordArguments,
                                  const RuntimeValue& localGraph);

}  // namespace ludork::engine::runtime_detail
