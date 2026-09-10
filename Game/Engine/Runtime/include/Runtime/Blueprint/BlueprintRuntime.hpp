#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <functional>
#include <string>

class LUDORK_RUNTIME_API BlueprintRuntimeFacade {
public:
    void setObjectGraphResolver(
        std::function<RuntimeValue(const RuntimeValue&)> resolver) const;
    void validateEvent(const RuntimeValue& object,
                       const std::string& eventName) const;
    void dispatchEvent(const RuntimeValue& object,
                       const RuntimeIdentityPtr& objectType,
                       const std::string& eventName,
                       const RuntimeValue& keywordArguments,
                       const RuntimeIdentityPtr& onComplete) const;
    bool hasEvent(const RuntimeValue& object,
                  const std::string& eventName) const;
    bool classHasEvent(const RuntimeIdentityPtr& classType,
                       const std::string& eventName) const;
    bool graphHasExecutableEvent(const RuntimeIdentityPtr& graph,
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
    bool executeGraph(const RuntimeIdentityPtr& graph,
                      const std::string& eventName,
                      const RuntimeValue& keywordArguments,
                      const RuntimeIdentityPtr& localGraph,
                      const RuntimeIdentityPtr& onComplete) const;
};

LUDORK_RUNTIME_API BlueprintRuntimeFacade& blueprintRuntime();
