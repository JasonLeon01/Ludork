#include <Runtime/BlueprintRuntime.hpp>

#include "EngineRuntimeSession.hpp"
#include "RuntimeSubsystemServices.hpp"

#include <LudorkCoreBinding/DynamicValueCodec.hpp>

namespace {

sol::object runtimeValue(sol::state_view lua, const RuntimeValue& value) {
    return ludork_core::writeLuaValue(lua, value);
}

sol::object runtimeIdentity(sol::state_view lua,
                            const RuntimeIdentityPtr& value) {
    return ludork_core::writeLuaValue(lua, value);
}

}  // namespace

void BlueprintRuntimeFacade::dispatchEvent(
    const RuntimeValue& object, const RuntimeIdentityPtr& objectType,
    const std::string& eventName, const RuntimeValue& keywordArguments,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    ludork::engine::runtime_detail::dispatchBlueprintEvent(
        sol::this_state(runtime.state()), runtimeValue(lua, object),
        runtimeIdentity(lua, objectType), eventName,
        runtimeValue(lua, keywordArguments),
        ludork::engine::runtime_detail::completionCallback(
            runtimeIdentity(lua, onComplete)));
}

bool BlueprintRuntimeFacade::hasEvent(const RuntimeValue& object,
                                      const std::string& eventName) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    return ludork::engine::runtime_detail::hasBlueprintEvent(
        sol::this_state(runtime.state()), runtimeValue(runtime.lua(), object),
        eventName);
}

bool BlueprintRuntimeFacade::classHasEvent(const RuntimeIdentityPtr& classType,
                                           const std::string& eventName) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    return ludork::engine::runtime_detail::classHasBlueprintEvent(
        sol::this_state(runtime.state()),
        runtimeIdentity(runtime.lua(), classType), eventName);
}

bool BlueprintRuntimeFacade::graphHasExecutableEvent(
    const RuntimeIdentityPtr& graph, const std::string& eventName) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    return ludork::engine::runtime_detail::blueprintGraphHasExecutableEvent(
        runtime.lua(), runtimeIdentity(runtime.lua(), graph), eventName);
}

bool BlueprintRuntimeFacade::graphDataHasExecutableEvent(
    const RuntimeValue& graphData, const std::string& eventName) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    return ludork::engine::runtime_detail::blueprintGraphDataHasExecutableEvent(
        runtime.lua(), runtimeValue(runtime.lua(), graphData), eventName);
}

bool BlueprintRuntimeFacade::executeParentEvent(
    const RuntimeValue& object, const RuntimeIdentityPtr& classType,
    const std::string& eventName, const RuntimeValue& arguments,
    const RuntimeValue& keywordArguments, const RuntimeIdentityPtr& localGraph,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::engine::runtime_detail::executeParentBlueprintEvent(
        sol::this_state(runtime.state()), runtimeValue(lua, object),
        runtimeIdentity(lua, classType), eventName,
        runtimeValue(lua, arguments), runtimeValue(lua, keywordArguments),
        runtimeIdentity(lua, localGraph),
        ludork::engine::runtime_detail::completionCallback(
            runtimeIdentity(lua, onComplete)));
}

bool BlueprintRuntimeFacade::executeGraph(
    const RuntimeIdentityPtr& graph, const std::string& eventName,
    const RuntimeValue& keywordArguments, const RuntimeIdentityPtr& localGraph,
    const RuntimeIdentityPtr& onComplete) const {
    ludork::engine::runtime_detail::EngineRuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return ludork::engine::runtime_detail::executeBlueprintGraph(
        lua, runtimeIdentity(lua, graph), eventName,
        runtimeValue(lua, keywordArguments), runtimeIdentity(lua, localGraph),
        ludork::engine::runtime_detail::completionCallback(
            runtimeIdentity(lua, onComplete)));
}

BlueprintRuntimeFacade& blueprintRuntime() {
    static BlueprintRuntimeFacade runtime;
    return runtime;
}
