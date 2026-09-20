#pragma once
#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/NodeGraph/Graph.hpp>

BIND_CLASS()
class LUDORK_RUNTIME_API BPBase {
public:
    BIND_INIT()
    BPBase() = default;

    virtual ~BPBase() = default;

    BIND_METHOD(defaults = {nil, nil})
    static void BlueprintEvent(
        const RuntimeIdentityPtr& object, const RuntimeIdentityPtr& objectType,
        const std::string& eventName,
        const RuntimeIdentityPtr& keywordArguments = nullptr,
        const RuntimeIdentityPtr& onComplete = nullptr);

    BIND_METHOD()
    static bool HasBlueprintEvent(const RuntimeIdentityPtr& object,
                                  const std::string& eventName);

    BIND_METHOD()
    static bool IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                      const std::string& eventName);

    BIND_METHOD(name = "_classHasBlueprintEvent", metadata = false)
    static bool ClassHasBlueprintEvent(const RuntimeIdentityPtr& classType,
                                       const std::string& eventName);

    BIND_METHOD(name = "_graphHasExecutableEvent", metadata = false,
                allow_nil = "graph")
    static bool GraphHasExecutableEvent(const std::shared_ptr<Graph>& graph,
                                        const std::string& eventName);

    BIND_METHOD(name = "_graphDataHasExecutableEvent", metadata = false)
    static bool GraphDataHasExecutableEvent(const RuntimeValue& graphData,
                                            const std::string& eventName);

    BIND_METHOD(defaults = {nil, nil, nil, nil})
    static bool ExecuteParentEvent(
        const RuntimeIdentityPtr& object, const RuntimeIdentityPtr& classType,
        const std::string& eventName,
        const RuntimeIdentityPtr& arguments = nullptr,
        const RuntimeIdentityPtr& keywordArguments = nullptr,
        const RuntimeIdentityPtr& localGraph = nullptr,
        const RuntimeIdentityPtr& onComplete = nullptr);

    BIND_METHOD(defaults = {nil, nil, nil}, metadata = false)
    static bool ExecuteGraph(
        const std::shared_ptr<Graph>& graph, const std::string& eventName,
        const RuntimeIdentityPtr& keywordArguments = nullptr,
        const RuntimeIdentityPtr& localGraph = nullptr,
        const RuntimeIdentityPtr& onComplete = nullptr);

    static void BlueprintEventNative(RuntimeObject& object,
                                     const std::string& eventName);
    static void BlueprintEventNative(RuntimeObject& object,
                                     const std::string& eventName,
                                     RuntimeValue::Map keywordArguments);

    static bool HasBlueprintEventNative(const RuntimeObject& object,
                                        const std::string& eventName);

private:
    static RuntimeValue objectValue(const RuntimeObject& object);
};
