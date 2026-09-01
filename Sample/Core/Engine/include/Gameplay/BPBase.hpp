#pragma once

#include <BindAnnotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>

BIND_CLASS()
class BPBase {
public:
    BIND_INIT()
    BPBase() = default;

    virtual ~BPBase() = default;

    BIND_METHOD(defaults = {nil, nil})
    static void BlueprintEvent(const RuntimeIdentityPtr& object,
                               const RuntimeIdentityPtr& objectType,
                               const std::string& eventName,
                               const RuntimeValue& keywordArguments = {},
                               const RuntimeIdentityPtr& onComplete = nullptr);

    BIND_METHOD()
    static bool HasBlueprintEvent(const RuntimeIdentityPtr& object,
                                  const std::string& eventName);

    BIND_METHOD()
    static bool IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                      const std::string& eventName);

    BIND_METHOD(metadata = false)
    static bool _classHasBlueprintEvent(const RuntimeIdentityPtr& classType,
                                        const std::string& eventName);

    BIND_METHOD(metadata = false)
    static bool _graphHasExecutableEvent(const RuntimeIdentityPtr& graph,
                                         const std::string& eventName);

    BIND_METHOD(metadata = false)
    static bool _graphDataHasExecutableEvent(const RuntimeValue& graphData,
                                             const std::string& eventName);

    BIND_METHOD(defaults = {nil, nil, nil, nil})
    static bool ExecuteParentEvent(
        const RuntimeIdentityPtr& object, const RuntimeIdentityPtr& classType,
        const std::string& eventName, const RuntimeValue& arguments = {},
        const RuntimeValue& keywordArguments = {},
        const RuntimeIdentityPtr& localGraph = nullptr,
        const RuntimeIdentityPtr& onComplete = nullptr);

    BIND_METHOD(defaults = {nil, nil, nil}, metadata = false)
    static bool ExecuteGraph(const RuntimeIdentityPtr& graph,
                             const std::string& eventName,
                             const RuntimeValue& keywordArguments = {},
                             const RuntimeIdentityPtr& localGraph = nullptr,
                             const RuntimeIdentityPtr& onComplete = nullptr);

    static void BlueprintEventNative(
        RuntimeObject& object, const std::string& eventName,
        const RuntimeValue::Map& keywordArguments = {});

    static bool HasBlueprintEventNative(const RuntimeObject& object,
                                        const std::string& eventName);

private:
    static RuntimeValue objectValue(const RuntimeObject& object);
};
