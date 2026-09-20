#pragma once

#include <EngineRuntimeApi.hpp>
#include <EngineDataProviders.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>
#include <Runtime/RuntimeIdentity.hpp>

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API RuntimeProviders {
public:
    BIND_METHOD(metadata = false)
    static void installData(
        EngineDataProviders::CurveResolver curveResolver,
        EngineDataProviders::TextConfigResolver plainTextConfigResolver);

    BIND_METHOD(metadata = false)
    static void installBlueprint(
        const RuntimeIdentityPtr& classDataByPath,
        const RuntimeIdentityPtr& compileGraph,
        const RuntimeIdentityPtr& instantiateGraphTemplate);

    BIND_METHOD(metadata = false)
    static void installConfig(const RuntimeIdentityPtr& configResolver);
};
