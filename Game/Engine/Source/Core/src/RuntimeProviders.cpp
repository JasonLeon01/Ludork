#include <RuntimeProviders.hpp>

#include <EngineDataProviders.hpp>
#include <Runtime/RuntimeProviderFacade.hpp>

#include <utility>

void RuntimeProviders::installData(
    EngineDataProviders::CurveResolver curveResolver,
    EngineDataProviders::TextConfigResolver plainTextConfigResolver) {
    EngineDataProviders::install(std::move(curveResolver),
                                 std::move(plainTextConfigResolver));
}

void RuntimeProviders::installBlueprint(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate) {
    ludork::runtime::detail::installBlueprintRuntimeProviders(
        classDataByPath, compileGraph, instantiateGraphTemplate);
}

void RuntimeProviders::installConfig(const RuntimeIdentityPtr& configResolver) {
    ludork::runtime::detail::installConfigRuntimeProvider(configResolver);
}
