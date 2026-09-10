#include <Runtime/RuntimeProviderFacade.hpp>

#include "RuntimeProviderInternals.hpp"
#include <Runtime/RuntimeSession.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>

#include <stdexcept>

namespace {

RuntimeIdentityPtr invokeIdentityProvider(
    ludork::runtime::detail::RuntimeProviderSlot slot,
    const std::vector<RuntimeIdentityPtr>& identityArguments,
    const std::vector<std::string>& stringArguments = {}) {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    std::vector<sol::object> arguments;
    arguments.reserve(identityArguments.size() + stringArguments.size());
    for (const RuntimeIdentityPtr& argument : identityArguments) {
        arguments.push_back(
            ludork::runtime::binding::writeOpaqueIdentity(lua, argument));
    }
    for (const std::string& argument : stringArguments) {
        arguments.push_back(sol::make_object(lua, argument));
    }
    return ludork::runtime::binding::readOpaqueIdentity<RuntimeIdentityPtr>(
        ludork::runtime::detail::invokeRuntimeProviderOne(lua, slot,
                                                          arguments));
}

}  // namespace

RuntimeIdentityPtr RuntimeProviderFacade::curve(const std::string& name) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::Curve, {}, {name});
}

RuntimeIdentityPtr RuntimeProviderFacade::plainTextConfig(
    const std::string& name) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::PlainTextConfig, {},
        {name});
}

RuntimeIdentityPtr RuntimeProviderFacade::blueprintClassData(
    const std::string& classPath) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::BlueprintClassDataByPath,
        {}, {classPath});
}

RuntimeIdentityPtr RuntimeProviderFacade::compileBlueprintGraph(
    const RuntimeIdentityPtr& graphData,
    const RuntimeIdentityPtr& classType) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::BlueprintCompileGraph,
        {graphData, classType});
}

RuntimeIdentityPtr RuntimeProviderFacade::instantiateBlueprintGraph(
    const RuntimeIdentityPtr& graphTemplate,
    const RuntimeIdentityPtr& parent) const {
    return invokeIdentityProvider(ludork::runtime::detail::RuntimeProviderSlot::
                                      BlueprintInstantiateGraphTemplate,
                                  {graphTemplate, parent});
}

std::string RuntimeProviderFacade::config(
    const std::string& configName, const std::string& settingName) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object value = ludork::runtime::detail::invokeRuntimeProviderOne(
        lua, ludork::runtime::detail::RuntimeProviderSlot::Config,
        {sol::make_object(lua, configName),
         sol::make_object(lua, settingName)});
    if (!value.is<std::string>()) {
        throw std::runtime_error(
            "Runtime config resolver must return a string");
    }
    return value.as<std::string>();
}

RuntimeProviderFacade& runtimeProviders() {
    static RuntimeProviderFacade providers;
    return providers;
}
