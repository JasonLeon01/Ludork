#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/RuntimeIdentity.hpp>

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
        const RuntimeIdentityPtr& compileGraph,
        const RuntimeIdentityPtr& instantiateGraphTemplate);

    BIND_METHOD(metadata = false)
    static void installConfig(const RuntimeIdentityPtr& configResolver);
};

namespace ludork::runtime::detail {

LUDORK_RUNTIME_API void clearRuntimeProviders() noexcept;

}  // namespace ludork::runtime::detail
