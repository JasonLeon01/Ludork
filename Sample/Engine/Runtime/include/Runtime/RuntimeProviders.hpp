#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>

BIND_CLASS(metadata = false)
class LUDORK_RUNTIME_API RuntimeProviders {
public:
    BIND_METHOD(metadata = false)
    static void installData(const RuntimeIdentityPtr& curveResolver,
                            const RuntimeIdentityPtr& plainTextConfigResolver);

    BIND_METHOD(metadata = false)
    static void installBlueprint(
        const RuntimeIdentityPtr& classDataByPath,
        const RuntimeIdentityPtr& invalidateClassData,
        const RuntimeIdentityPtr& compileGraph,
        const RuntimeIdentityPtr& instantiateGraphTemplate);

    BIND_METHOD(metadata = false)
    static void installConfig(const RuntimeIdentityPtr& configResolver);
};

class LUDORK_RUNTIME_API RuntimeProviderFacade {
public:
    RuntimeIdentityPtr curve(const std::string& name) const;
    RuntimeIdentityPtr plainTextConfig(const std::string& name) const;
    RuntimeIdentityPtr blueprintClassData(const std::string& classPath) const;
    void invalidateBlueprintClassData(const std::string& classPath) const;
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

namespace ludork::runtime::detail {

LUDORK_RUNTIME_API void clearRuntimeProviders() noexcept;

}  // namespace ludork::runtime::detail
