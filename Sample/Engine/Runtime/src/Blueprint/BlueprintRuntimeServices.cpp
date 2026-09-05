#include <Runtime/RuntimeReference.hpp>
#include <Runtime/Blueprint/BlueprintRuntime.hpp>

#include "BlueprintRuntime/BlueprintRuntimeInternal.hpp"

#include <Runtime/RuntimeSession.hpp>

#include <utility>

using namespace ludork::runtime::reference;

void BlueprintRuntimeFacade::setObjectGraphResolver(
    std::function<RuntimeValue(const RuntimeValue&)> resolver) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::blueprint_detail::objectGraphResolver() =
        std::move(resolver);
}

void BlueprintRuntimeFacade::validateEvent(const RuntimeValue& object,
                                           const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::blueprint_detail::validateBlueprintEvent(retain(object),
                                                              eventName);
}

void BlueprintRuntimeFacade::dispatchEvent(
    const RuntimeValue& object, const RuntimeIdentityPtr& objectType,
    const std::string& eventName, const RuntimeValue& keywordArguments,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::blueprint_detail::dispatchBlueprintEvent(
        retain(object), retain(makeValue(objectType)), eventName,
        retain(keywordArguments),
        ludork::runtime::blueprint_detail::completionCallback(
            retain(makeValue(onComplete))));
}

bool BlueprintRuntimeFacade::hasEvent(const RuntimeValue& object,
                                      const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::hasBlueprintEvent(retain(object),
                                                                eventName);
}

bool BlueprintRuntimeFacade::classHasEvent(const RuntimeIdentityPtr& classType,
                                           const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::classHasBlueprintEvent(
        retain(makeValue(classType)), eventName);
}

bool BlueprintRuntimeFacade::graphHasExecutableEvent(
    const RuntimeIdentityPtr& graph, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::blueprintGraphHasExecutableEvent(
        retain(makeValue(graph)), eventName);
}

bool BlueprintRuntimeFacade::graphDataHasExecutableEvent(
    const RuntimeValue& graphData, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::
        blueprintGraphDataHasExecutableEvent(retain(graphData), eventName);
}

bool BlueprintRuntimeFacade::executeParentEvent(
    const RuntimeValue& object, const RuntimeIdentityPtr& classType,
    const std::string& eventName, const RuntimeValue& arguments,
    const RuntimeValue& keywordArguments, const RuntimeIdentityPtr& localGraph,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::executeParentBlueprintEvent(
        retain(object), retain(makeValue(classType)), eventName,
        retain(arguments), retain(keywordArguments),
        retain(makeValue(localGraph)),
        ludork::runtime::blueprint_detail::completionCallback(
            retain(makeValue(onComplete))));
}

bool BlueprintRuntimeFacade::executeGraph(
    const RuntimeIdentityPtr& graph, const std::string& eventName,
    const RuntimeValue& keywordArguments, const RuntimeIdentityPtr& localGraph,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::executeBlueprintGraph(
        retain(makeValue(graph)), eventName, retain(keywordArguments),
        retain(makeValue(localGraph)),
        ludork::runtime::blueprint_detail::completionCallback(
            retain(makeValue(onComplete))));
}

BlueprintRuntimeFacade& blueprintRuntime() {
    static BlueprintRuntimeFacade runtime;
    return runtime;
}
