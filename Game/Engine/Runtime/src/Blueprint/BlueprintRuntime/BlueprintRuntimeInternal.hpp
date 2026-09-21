#pragma once

#include <Runtime/RuntimeReference.hpp>
#include <Runtime/Blueprint/BlueprintRuntime.hpp>

#include <memory>
#include <unordered_set>

namespace ludork::runtime::blueprint_detail {

struct EventDescriptor {
    std::vector<std::string> parameters;
    std::unordered_set<std::string> accepted;
    bool metadataFound = false;
};

using EventArguments = std::vector<BlueprintRuntimeFacade::EventArgument>;
using ObjectGraphResolver = std::function<std::shared_ptr<Graph>(
    const std::shared_ptr<RuntimeObject>&)>;

std::shared_ptr<Graph> requireBlueprintGraph(const RuntimeValue& graph);
EventArguments eventArguments(const RuntimeValue& keywordArguments);
std::function<void()> completionCallback(const RuntimeValue& value);
bool hasBlueprintEvent(const RuntimeValue& object, const std::string& eventName,
                       const RuntimeHandle& classEventCache = {});
std::vector<bool> hasBlueprintEvents(const RuntimeValue& object,
                                     const std::vector<std::string>& eventNames,
                                     const RuntimeHandle& classEventCache);
void dispatchBlueprintEvent(const RuntimeValue& object,
                            const RuntimeValue& rawObjectType,
                            const std::string& eventName,
                            const EventArguments& arguments,
                            const std::function<void()>& onComplete,
                            const RuntimeHandle& keywordSource = {});
void validateBlueprintEvent(const RuntimeValue& object,
                            const std::string& eventName);
bool blueprintGraphHasExecutableEvent(const std::shared_ptr<Graph>& graph,
                                      const std::string& eventName);
bool blueprintGraphDataHasExecutableEvent(const RuntimeValue& graphData,
                                          const std::string& eventName);
bool classHasBlueprintEvent(const RuntimeValue& rawClass,
                            const std::string& eventName,
                            const RuntimeHandle& classEventCache = {});
bool executeParentBlueprintEvent(const RuntimeValue& object,
                                 const RuntimeValue& rawObjectClass,
                                 const std::string& eventName,
                                 const RuntimeValue& positionalArguments,
                                 const EventArguments& arguments,
                                 const RuntimeValue& localGraph,
                                 const std::function<void()>& onComplete);
bool executeBlueprintGraph(const std::shared_ptr<Graph>& graph,
                           const std::string& eventName,
                           const EventArguments& arguments,
                           const RuntimeValue& localGraph,
                           const std::function<void()>& onComplete);
void clearBlueprintRuntimeCaches(lua_State* state) noexcept;

inline constexpr const char* BLUEPRINT_IMPLEMENTATION_CACHE_KEY =
    "Ludork.Runtime.blueprintImplementationCache";
inline constexpr const char* BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY =
    "Ludork.Runtime.blueprintEventDescriptorCache";
inline constexpr const char* BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY =
    "Ludork.Runtime.blueprintCallableParameterCache";

void invokeCompletion(const std::function<void()>& callback);
std::shared_ptr<const EventDescriptor> runtimeEventDescriptor(
    const RuntimeValue& method, const RuntimeValue& classType,
    const std::string& eventName);
void invokeNamedRuntimeMethod(const RuntimeValue& object,
                              const RuntimeValue& method,
                              const RuntimeValue& classType,
                              const std::string& eventName,
                              const EventArguments& arguments,
                              const RuntimeHandle& keywordSource = {});
bool runtimeMethodHasImplementation(const RuntimeValue& method);
ObjectGraphResolver& objectGraphResolver();
std::shared_ptr<Graph> objectGraph(const RuntimeValue& object);
bool blueprintIsInstance(const RuntimeValue& value, const RuntimeValue& type);
RuntimeValue callRuntimeMethodFirst(
    const RuntimeValue& object, const char* name,
    const std::vector<RuntimeValue>& arguments = {});
bool generatedBlueprintGraphHasExecutableEvent(const RuntimeValue& classType,
                                               const std::string& eventName);
std::shared_ptr<Graph> generatedBlueprintGraph(const RuntimeValue& object,
                                               const RuntimeValue& classType);
EventArguments blueprintEventArguments(const RuntimeValue& classType,
                                       const std::string& eventName,
                                       const RuntimeValue& rawArguments,
                                       const EventArguments& arguments);
void mergeBlueprintLocalArguments(const RuntimeValue& classType,
                                  const std::string& eventName,
                                  EventArguments& arguments,
                                  const RuntimeValue& localGraph);

}  // namespace ludork::runtime::blueprint_detail
