#include <Runtime/Blueprint/BPBase.hpp>
#include <Runtime/Blueprint/BlueprintRuntime.hpp>

#include <utility>

void BPBase::BlueprintEvent(const RuntimeIdentityPtr& object,
                            const RuntimeIdentityPtr& objectType,
                            const std::string& eventName,
                            const RuntimeIdentityPtr& keywordArguments,
                            const RuntimeIdentityPtr& onComplete) {
    blueprintRuntime().dispatchEvent(RuntimeValue(object), objectType,
                                     eventName, RuntimeValue(keywordArguments),
                                     onComplete);
}

bool BPBase::HasBlueprintEvent(const RuntimeIdentityPtr& object,
                               const std::string& eventName) {
    return blueprintRuntime().hasEvent(RuntimeValue(object), eventName);
}

bool BPBase::IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                   const std::string& eventName) {
    return !HasBlueprintEvent(object, eventName);
}

bool BPBase::ClassHasBlueprintEvent(const RuntimeIdentityPtr& classType,
                                    const std::string& eventName) {
    return blueprintRuntime().classHasEvent(classType, eventName);
}

bool BPBase::GraphHasExecutableEvent(const RuntimeIdentityPtr& graph,
                                     const std::string& eventName) {
    return blueprintRuntime().graphHasExecutableEvent(graph, eventName);
}

bool BPBase::GraphDataHasExecutableEvent(const RuntimeValue& graphData,
                                         const std::string& eventName) {
    return blueprintRuntime().graphDataHasExecutableEvent(graphData, eventName);
}

bool BPBase::ExecuteParentEvent(const RuntimeIdentityPtr& object,
                                const RuntimeIdentityPtr& classType,
                                const std::string& eventName,
                                const RuntimeIdentityPtr& arguments,
                                const RuntimeIdentityPtr& keywordArguments,
                                const RuntimeIdentityPtr& localGraph,
                                const RuntimeIdentityPtr& onComplete) {
    return blueprintRuntime().executeParentEvent(
        RuntimeValue(object), classType, eventName, RuntimeValue(arguments),
        RuntimeValue(keywordArguments), localGraph, onComplete);
}

bool BPBase::ExecuteGraph(const RuntimeIdentityPtr& graph,
                          const std::string& eventName,
                          const RuntimeIdentityPtr& keywordArguments,
                          const RuntimeIdentityPtr& localGraph,
                          const RuntimeIdentityPtr& onComplete) {
    return blueprintRuntime().executeGraph(graph, eventName,
                                           RuntimeValue(keywordArguments),
                                           localGraph, onComplete);
}

void BPBase::BlueprintEventNative(RuntimeObject& object,
                                  const std::string& eventName) {
    const RuntimeValue runtimeObject = objectValue(object);
    if (runtimeObject.isNil()) {
        return;
    }
    blueprintRuntime().dispatchEvent(runtimeObject, nullptr, eventName,
                                     RuntimeValue(), nullptr);
}

void BPBase::BlueprintEventNative(RuntimeObject& object,
                                  const std::string& eventName,
                                  RuntimeValue::Map keywordArguments) {
    const RuntimeValue runtimeObject = objectValue(object);
    if (runtimeObject.isNil()) {
        return;
    }
    blueprintRuntime().dispatchEvent(runtimeObject, nullptr, eventName,
                                     RuntimeValue(std::move(keywordArguments)),
                                     nullptr);
}

bool BPBase::HasBlueprintEventNative(const RuntimeObject& object,
                                     const std::string& eventName) {
    const RuntimeValue runtimeObject = objectValue(object);
    return !runtimeObject.isNil() &&
           blueprintRuntime().hasEvent(runtimeObject, eventName);
}

RuntimeValue BPBase::objectValue(const RuntimeObject& object) {
    std::shared_ptr<RuntimeObject> owner = object.runtimeOwner();
    if (!owner) {
        std::shared_ptr<const RuntimeObject> constOwner =
            object.weak_from_this().lock();
        owner = std::const_pointer_cast<RuntimeObject>(std::move(constOwner));
    }
    return owner ? RuntimeValue(std::move(owner)) : RuntimeValue();
}
