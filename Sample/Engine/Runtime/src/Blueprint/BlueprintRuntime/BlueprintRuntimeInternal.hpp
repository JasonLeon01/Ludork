#pragma once

#include <Runtime/RuntimeReference.hpp>

class Graph;

namespace ludork::runtime::blueprint_detail {

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
    "Ludork.Runtime.blueprintImplementationCache";
inline constexpr const char* BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY =
    "Ludork.Runtime.blueprintEventDescriptorCache";
inline constexpr const char* BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY =
    "Ludork.Runtime.blueprintCallableParameterCache";

void invokeCompletion(const std::function<void()>& callback);
RuntimeHandle classRuntimeEventCache(const RuntimeValue& classType);
RuntimeHandle runtimeEventDescriptor(const RuntimeValue& method,
                                     const RuntimeValue& classType,
                                     const std::string& eventName);
RuntimeHandle runtimeDescriptorParameters(const RuntimeValue& descriptor);
void invokeNamedRuntimeMethod(const RuntimeValue& object,
                              const RuntimeValue& method,
                              const RuntimeValue& classType,
                              const std::string& eventName,
                              const RuntimeValue& rawKeywordArguments);
bool runtimeMethodHasImplementation(const RuntimeValue& method);
std::function<RuntimeValue(const RuntimeValue&)>& objectGraphResolver();
RuntimeValue objectGraph(const RuntimeValue& object);
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

}  // namespace ludork::runtime::blueprint_detail
