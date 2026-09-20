#include <Runtime/RuntimeProviderFacade.hpp>

#include "RuntimeProviderImpl.hpp"

RuntimeIdentityPtr RuntimeProviderFacade::blueprintClassData(
    const std::string& classPath) const {
    return ludork::runtime::detail::RuntimeProviderImpl::instance().classData(
        classPath);
}

std::shared_ptr<Graph> RuntimeProviderFacade::compileBlueprintGraph(
    const RuntimeIdentityPtr& graphData,
    const RuntimeIdentityPtr& classType) const {
    return ludork::runtime::detail::RuntimeProviderImpl::instance()
        .compileGraph(graphData, classType);
}

std::shared_ptr<Graph> RuntimeProviderFacade::instantiateBlueprintGraph(
    const std::shared_ptr<Graph>& graphTemplate,
    const RuntimeIdentityPtr& parent) const {
    return ludork::runtime::detail::RuntimeProviderImpl::instance()
        .instantiateGraph(graphTemplate, parent);
}

std::string RuntimeProviderFacade::config(
    const std::string& configName, const std::string& settingName) const {
    return ludork::runtime::detail::RuntimeProviderImpl::instance().config(
        configName, settingName);
}

RuntimeProviderFacade& runtimeProviders() {
    static RuntimeProviderFacade providers;
    return providers;
}

namespace ludork::runtime::detail {

void installBlueprintRuntimeProviders(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate) {
    RuntimeProviderImpl::instance().installBlueprint(
        classDataByPath, compileGraph, instantiateGraphTemplate);
}

void installConfigRuntimeProvider(const RuntimeIdentityPtr& configResolver) {
    RuntimeProviderImpl::instance().installConfig(configResolver);
}

void clearRuntimeProviders() noexcept {
    RuntimeProviderImpl::instance().clear();
}

}  // namespace ludork::runtime::detail
