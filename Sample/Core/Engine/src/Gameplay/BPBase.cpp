#include <Gameplay/BPBase.hpp>

#include <utility>

namespace {

std::vector<RuntimeValue> callBlueprintRuntime(
    const std::string& operation, std::vector<RuntimeValue> arguments) {
    return resolveRuntime("blueprint." + operation, arguments);
}

}  // namespace

void BPBase::BlueprintEvent(const RuntimeIdentityPtr& object,
                            const RuntimeIdentityPtr& objectType,
                            const std::string& eventName,
                            const RuntimeValue& keywordArguments,
                            const RuntimeIdentityPtr& onComplete) {
    callBlueprintRuntime(
        "BlueprintEvent",
        {RuntimeValue(object), RuntimeValue(objectType),
         RuntimeValue(eventName), keywordArguments, RuntimeValue(onComplete)});
}

bool BPBase::HasBlueprintEvent(const RuntimeIdentityPtr& object,
                               const std::string& eventName) {
    return booleanResult("HasBlueprintEvent",
                         {RuntimeValue(object), RuntimeValue(eventName)});
}

bool BPBase::IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                   const std::string& eventName) {
    return !HasBlueprintEvent(object, eventName);
}

bool BPBase::_tryExecuteInfoGraph(const RuntimeIdentityPtr& object,
                                  const std::string& eventName,
                                  const RuntimeValue& keywordArguments,
                                  const RuntimeIdentityPtr& onComplete) {
    return booleanResult("_tryExecuteInfoGraph",
                         {RuntimeValue(object), RuntimeValue(eventName),
                          keywordArguments, RuntimeValue(onComplete)});
}

bool BPBase::_classHasBlueprintEvent(const RuntimeIdentityPtr& classType,
                                     const std::string& eventName) {
    return booleanResult("_classHasBlueprintEvent",
                         {RuntimeValue(classType), RuntimeValue(eventName)});
}

bool BPBase::_graphHasExecutableEvent(const RuntimeIdentityPtr& graph,
                                      const std::string& eventName) {
    return booleanResult("_graphHasExecutableEvent",
                         {RuntimeValue(graph), RuntimeValue(eventName)});
}

bool BPBase::_graphDataHasExecutableEvent(const RuntimeValue& graphData,
                                          const std::string& eventName) {
    return booleanResult("_graphDataHasExecutableEvent",
                         {graphData, RuntimeValue(eventName)});
}

bool BPBase::ExecuteParentEvent(const RuntimeIdentityPtr& object,
                                const RuntimeIdentityPtr& classType,
                                const std::string& eventName,
                                const RuntimeValue& arguments,
                                const RuntimeValue& keywordArguments,
                                const RuntimeIdentityPtr& localGraph,
                                const RuntimeIdentityPtr& onComplete) {
    return booleanResult(
        "ExecuteParentEvent",
        {RuntimeValue(object), RuntimeValue(classType), RuntimeValue(eventName),
         arguments, keywordArguments,
         localGraph == nullptr ? RuntimeValue() : RuntimeValue(localGraph),
         RuntimeValue(onComplete)});
}

bool BPBase::_executeGraph(const RuntimeIdentityPtr& graph,
                           const std::string& eventName,
                           const RuntimeValue& keywordArguments,
                           const RuntimeIdentityPtr& localGraph,
                           const RuntimeIdentityPtr& onComplete) {
    return booleanResult(
        "_executeGraph",
        {RuntimeValue(graph), RuntimeValue(eventName), keywordArguments,
         localGraph == nullptr ? RuntimeValue() : RuntimeValue(localGraph),
         RuntimeValue(onComplete)});
}

void BPBase::ExecuteInfoGraph(const RuntimeIdentityPtr& object,
                              const std::string& eventName,
                              const RuntimeValue& keywordArguments) {
    callBlueprintRuntime(
        "ExecuteInfoGraph",
        {RuntimeValue(object), RuntimeValue(eventName), keywordArguments});
}

RuntimeValue BPBase::_resolveGeneralDataDict(const RuntimeValue& value) {
    std::vector<RuntimeValue> result =
        callBlueprintRuntime("_resolveGeneralDataDict", {value});
    return result.empty() ? RuntimeValue() : std::move(result.front());
}

void BPBase::ApplyGeneralData(const RuntimeIdentityPtr& object,
                              const RuntimeValue& data,
                              const RuntimeValue& parameterTypes) {
    callBlueprintRuntime("ApplyGeneralData",
                         {RuntimeValue(object), data, parameterTypes});
}

void BPBase::BlueprintEventNative(RuntimeObject& object,
                                  const std::string& eventName,
                                  const RuntimeValue::Map& keywordArguments) {
    const RuntimeValue runtimeObject = objectValue(object);
    if (runtimeObject.isNil()) {
        return;
    }
    callBlueprintRuntime("BlueprintEvent", {runtimeObject, RuntimeValue(),
                                            RuntimeValue(eventName),
                                            RuntimeValue(keywordArguments)});
}

bool BPBase::HasBlueprintEventNative(const RuntimeObject& object,
                                     const std::string& eventName) {
    const RuntimeValue runtimeObject = objectValue(object);
    return !runtimeObject.isNil() &&
           booleanResult("HasBlueprintEvent",
                         {runtimeObject, RuntimeValue(eventName)});
}

bool BPBase::TryExecuteInfoGraphNative(
    RuntimeObject& object, const std::string& eventName,
    const RuntimeValue::Map& keywordArguments) {
    const RuntimeValue runtimeObject = objectValue(object);
    return !runtimeObject.isNil() &&
           booleanResult("_tryExecuteInfoGraph",
                         {runtimeObject, RuntimeValue(eventName),
                          RuntimeValue(keywordArguments)});
}

void BPBase::ApplyGeneralDataNative(RuntimeObject& object,
                                    const RuntimeValue& data,
                                    const RuntimeValue& parameterTypes) {
    const RuntimeValue runtimeObject = objectValue(object);
    if (!runtimeObject.isNil()) {
        callBlueprintRuntime("ApplyGeneralData",
                             {runtimeObject, data, parameterTypes});
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

bool BPBase::booleanResult(const std::string& operation,
                           const std::vector<RuntimeValue>& arguments) {
    const std::vector<RuntimeValue> result =
        callBlueprintRuntime(operation, arguments);
    if (result.empty()) {
        return false;
    }
    const bool* value = result.front().getIf<bool>();
    return value != nullptr && *value;
}
