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
    ludork::runtime::blueprint_detail::validateBlueprintEvent(intern(object),
                                                              eventName);
}

void BlueprintRuntimeFacade::dispatchEvent(
    const RuntimeValue& object, const RuntimeIdentityPtr& objectType,
    const std::string& eventName, const RuntimeValue& keywordArguments,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::runtime::RuntimeScope runtime;
    ludork::runtime::blueprint_detail::dispatchBlueprintEvent(
        intern(object), RuntimeValue(objectType), eventName, keywordArguments,
        ludork::runtime::blueprint_detail::completionCallback(
            RuntimeValue(onComplete)));
}

bool BlueprintRuntimeFacade::hasEvent(const RuntimeValue& object,
                                      const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::hasBlueprintEvent(intern(object),
                                                                eventName);
}

bool BlueprintRuntimeFacade::classHasEvent(const RuntimeIdentityPtr& classType,
                                           const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::classHasBlueprintEvent(
        RuntimeValue(classType), eventName);
}

bool BlueprintRuntimeFacade::graphHasExecutableEvent(
    const RuntimeIdentityPtr& graph, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::blueprintGraphHasExecutableEvent(
        RuntimeValue(graph), eventName);
}

bool BlueprintRuntimeFacade::graphDataHasExecutableEvent(
    const RuntimeValue& graphData, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::
        blueprintGraphDataHasExecutableEvent(intern(graphData), eventName);
}

bool BlueprintRuntimeFacade::executeParentEvent(
    const RuntimeValue& object, const RuntimeIdentityPtr& classType,
    const std::string& eventName, const RuntimeValue& arguments,
    const RuntimeValue& keywordArguments, const RuntimeIdentityPtr& localGraph,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::executeParentBlueprintEvent(
        intern(object), RuntimeValue(classType), eventName, arguments,
        keywordArguments, RuntimeValue(localGraph),
        ludork::runtime::blueprint_detail::completionCallback(
            RuntimeValue(onComplete)));
}

bool BlueprintRuntimeFacade::executeGraph(
    const RuntimeIdentityPtr& graph, const std::string& eventName,
    const RuntimeValue& keywordArguments, const RuntimeIdentityPtr& localGraph,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::runtime::RuntimeScope runtime;
    return ludork::runtime::blueprint_detail::executeBlueprintGraph(
        RuntimeValue(graph), eventName, keywordArguments,
        RuntimeValue(localGraph),
        ludork::runtime::blueprint_detail::completionCallback(
            RuntimeValue(onComplete)));
}

BlueprintRuntimeFacade& blueprintRuntime() {
    static BlueprintRuntimeFacade runtime;
    return runtime;
}
