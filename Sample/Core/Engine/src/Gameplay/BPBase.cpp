#include <Gameplay/BPBase.hpp>
#include <Runtime/BlueprintRuntime.hpp>

#include <utility>

void BPBase::BlueprintEvent(const RuntimeIdentityPtr& object,
                            const RuntimeIdentityPtr& objectType,
                            const std::string& eventName,
                            const RuntimeValue& keywordArguments,
                            const RuntimeIdentityPtr& onComplete) {
    blueprintRuntime().dispatchEvent(RuntimeValue(object), objectType,
                                     eventName, keywordArguments, onComplete);
}

bool BPBase::HasBlueprintEvent(const RuntimeIdentityPtr& object,
                               const std::string& eventName) {
    return blueprintRuntime().hasEvent(RuntimeValue(object), eventName);
}

bool BPBase::IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                   const std::string& eventName) {
    return !HasBlueprintEvent(object, eventName);
}

bool BPBase::_tryExecuteInfoGraph(const RuntimeIdentityPtr& object,
                                  const std::string& eventName,
                                  const RuntimeValue& keywordArguments,
                                  const RuntimeIdentityPtr& onComplete) {
    return blueprintRuntime().tryExecuteInfoGraph(
        RuntimeValue(object), eventName, keywordArguments, onComplete);
}

bool BPBase::_classHasBlueprintEvent(const RuntimeIdentityPtr& classType,
                                     const std::string& eventName) {
    return blueprintRuntime().classHasEvent(classType, eventName);
}

bool BPBase::_graphHasExecutableEvent(const RuntimeIdentityPtr& graph,
                                      const std::string& eventName) {
    return blueprintRuntime().graphHasExecutableEvent(graph, eventName);
}

bool BPBase::_graphDataHasExecutableEvent(const RuntimeValue& graphData,
                                          const std::string& eventName) {
    return blueprintRuntime().graphDataHasExecutableEvent(graphData, eventName);
}

bool BPBase::ExecuteParentEvent(const RuntimeIdentityPtr& object,
                                const RuntimeIdentityPtr& classType,
                                const std::string& eventName,
                                const RuntimeValue& arguments,
                                const RuntimeValue& keywordArguments,
                                const RuntimeIdentityPtr& localGraph,
                                const RuntimeIdentityPtr& onComplete) {
    return blueprintRuntime().executeParentEvent(
        RuntimeValue(object), classType, eventName, arguments, keywordArguments,
        localGraph, onComplete);
}

bool BPBase::_executeGraph(const RuntimeIdentityPtr& graph,
                           const std::string& eventName,
                           const RuntimeValue& keywordArguments,
                           const RuntimeIdentityPtr& localGraph,
                           const RuntimeIdentityPtr& onComplete) {
    return blueprintRuntime().executeGraph(graph, eventName, keywordArguments,
                                           localGraph, onComplete);
}

void BPBase::ExecuteInfoGraph(const RuntimeIdentityPtr& object,
                              const std::string& eventName,
                              const RuntimeValue& keywordArguments) {
    blueprintRuntime().executeInfoGraph(RuntimeValue(object), eventName,
                                        keywordArguments);
}

RuntimeValue BPBase::_resolveGeneralDataDict(const RuntimeValue& value) {
    return blueprintRuntime().resolveGeneralDataDictionary(value);
}

void BPBase::ApplyGeneralData(const RuntimeIdentityPtr& object,
                              const RuntimeValue& data,
                              const RuntimeValue& parameterTypes) {
    blueprintRuntime().applyGeneralData(RuntimeValue(object), data,
                                        parameterTypes);
}

void BPBase::BlueprintEventNative(RuntimeObject& object,
                                  const std::string& eventName,
                                  const RuntimeValue::Map& keywordArguments) {
    const RuntimeValue runtimeObject = objectValue(object);
    if (runtimeObject.isNil()) {
        return;
    }
    blueprintRuntime().dispatchEvent(runtimeObject, nullptr, eventName,
                                     RuntimeValue(keywordArguments), nullptr);
}

bool BPBase::HasBlueprintEventNative(const RuntimeObject& object,
                                     const std::string& eventName) {
    const RuntimeValue runtimeObject = objectValue(object);
    return !runtimeObject.isNil() &&
           blueprintRuntime().hasEvent(runtimeObject, eventName);
}

bool BPBase::TryExecuteInfoGraphNative(
    RuntimeObject& object, const std::string& eventName,
    const RuntimeValue::Map& keywordArguments) {
    const RuntimeValue runtimeObject = objectValue(object);
    return !runtimeObject.isNil() &&
           blueprintRuntime().tryExecuteInfoGraph(
               runtimeObject, eventName, RuntimeValue(keywordArguments),
               nullptr);
}

void BPBase::ApplyGeneralDataNative(RuntimeObject& object,
                                    const RuntimeValue& data,
                                    const RuntimeValue& parameterTypes) {
    const RuntimeValue runtimeObject = objectValue(object);
    if (!runtimeObject.isNil()) {
        blueprintRuntime().applyGeneralData(runtimeObject, data,
                                            parameterTypes);
    }
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
