#include <Runtime/RuntimeProviders.hpp>

#include "RuntimeProviderInternals.hpp"

void RuntimeProviders::installData(
    const RuntimeIdentityPtr& curveResolver,
    const RuntimeIdentityPtr& plainTextConfigResolver) {
    ludork::runtime::detail::installDataRuntimeProviders(
        curveResolver, plainTextConfigResolver);
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
