#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeIdentity.hpp>
#include <memory>
#include <string>

class Graph;

class LUDORK_RUNTIME_API RuntimeProviderFacade {
public:
    RuntimeIdentityPtr blueprintClassData(const std::string& classPath) const;
    std::shared_ptr<Graph> compileBlueprintGraph(
        const RuntimeIdentityPtr& graphData,
        const RuntimeIdentityPtr& classType) const;
    std::shared_ptr<Graph> instantiateBlueprintGraph(
        const std::shared_ptr<Graph>& graphTemplate,
        const RuntimeIdentityPtr& parent) const;
    std::string config(const std::string& configName,
                       const std::string& settingName) const;
};

LUDORK_RUNTIME_API RuntimeProviderFacade& runtimeProviders();

namespace ludork::runtime::detail {

LUDORK_RUNTIME_API void installBlueprintRuntimeProviders(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate);
LUDORK_RUNTIME_API void installConfigRuntimeProvider(
    const RuntimeIdentityPtr& configResolver);
LUDORK_RUNTIME_API void clearRuntimeProviders() noexcept;

}  // namespace ludork::runtime::detail
