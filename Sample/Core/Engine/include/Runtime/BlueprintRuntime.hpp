#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <vector>

class LUDORK_ENGINE_API BlueprintRuntimeFacade {
public:
    void dispatchEvent(const RuntimeValue& object,
                       const RuntimeIdentityPtr& objectType,
                       const std::string& eventName,
                       const RuntimeValue& keywordArguments,
                       const RuntimeIdentityPtr& onComplete) const;
    bool hasEvent(const RuntimeValue& object,
                  const std::string& eventName) const;
    bool tryExecuteInfoGraph(const RuntimeValue& object,
                             const std::string& eventName,
                             const RuntimeValue& keywordArguments,
                             const RuntimeIdentityPtr& onComplete) const;
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
    void executeInfoGraph(const RuntimeValue& object,
                          const std::string& eventName,
                          const RuntimeValue& keywordArguments) const;
    RuntimeValue resolveGeneralDataDictionary(const RuntimeValue& value) const;
    void applyGeneralData(const RuntimeValue& object, const RuntimeValue& data,
                          const RuntimeValue& parameterTypes) const;
    void initializeInfo(const RuntimeValue& object,
                        const RuntimeIdentityPtr& dataProvider) const;
    std::vector<std::string> registeredEvents(
        const RuntimeIdentityPtr& classType) const;
    std::string infoType(const RuntimeIdentityPtr& classType) const;
};

LUDORK_ENGINE_API BlueprintRuntimeFacade& blueprintRuntime();
