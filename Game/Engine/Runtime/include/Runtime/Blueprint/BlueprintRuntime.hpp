#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <functional>
#include <string>
#include <vector>

class Graph;
class RuntimeObject;

class LUDORK_RUNTIME_API BlueprintRuntimeFacade {
public:
    struct EventArgument {
        std::string name;
        RuntimeValue value;
    };

    void setObjectGraphResolver(std::function<std::shared_ptr<Graph>(
                                    const std::shared_ptr<RuntimeObject>&)>
                                    resolver) const;
    void validateEvent(const RuntimeValue& object,
                       const std::string& eventName) const;
    void dispatchEvent(const RuntimeValue& object,
                       const RuntimeIdentityPtr& objectType,
                       const std::string& eventName,
                       const RuntimeValue& keywordArguments,
                       const RuntimeIdentityPtr& onComplete) const;
    void dispatchEventArguments(
        const std::shared_ptr<RuntimeObject>& object,
        const std::string& eventName,
        const std::vector<EventArgument>& arguments) const;
    bool hasEvent(const RuntimeValue& object,
                  const std::string& eventName) const;
    bool classHasEvent(const RuntimeIdentityPtr& classType,
                       const std::string& eventName) const;
    bool graphHasExecutableEvent(const std::shared_ptr<Graph>& graph,
                                 const std::string& eventName) const;
    bool graphDataHasExecutableEvent(const RuntimeValue& graphData,
                                     const std::string& eventName) const;
    bool executeParentEvent(const RuntimeValue& object,
                            const RuntimeIdentityPtr& classType,
                            const std::string& eventName,
                            const RuntimeValue& arguments,
                            const RuntimeValue& keywordArguments,
                            const RuntimeIdentityPtr& localGraph,
                            const RuntimeIdentityPtr& onComplete) const;
    bool executeGraph(const std::shared_ptr<Graph>& graph,
                      const std::string& eventName,
                      const RuntimeValue& keywordArguments,
                      const RuntimeIdentityPtr& localGraph,
                      const RuntimeIdentityPtr& onComplete) const;
};

LUDORK_RUNTIME_API BlueprintRuntimeFacade& blueprintRuntime();
