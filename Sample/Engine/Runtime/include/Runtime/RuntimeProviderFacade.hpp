#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeIdentity.hpp>
#include <string>

class LUDORK_RUNTIME_API RuntimeProviderFacade {
public:
    RuntimeIdentityPtr curve(const std::string& name) const;
    RuntimeIdentityPtr plainTextConfig(const std::string& name) const;
    RuntimeIdentityPtr blueprintClassData(const std::string& classPath) const;
    RuntimeIdentityPtr compileBlueprintGraph(
        const RuntimeIdentityPtr& graphData,
        const RuntimeIdentityPtr& classType) const;
    RuntimeIdentityPtr instantiateBlueprintGraph(
        const RuntimeIdentityPtr& graphTemplate,
        const RuntimeIdentityPtr& parent) const;
    std::string config(const std::string& configName,
                       const std::string& settingName) const;
};

LUDORK_RUNTIME_API RuntimeProviderFacade& runtimeProviders();
