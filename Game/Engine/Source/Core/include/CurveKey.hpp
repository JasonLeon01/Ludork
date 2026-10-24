#pragma once

#include <CurveInterpolation.hpp>

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    float value = 0.0f;

    BIND_PROPERTY(default = "linear")
    std::string interpolation = CurveInterpolation::LINEAR;

    BIND_PROPERTY()
    float arriveTangent = 0.0f;

    BIND_PROPERTY()
    float leaveTangent = 0.0f;
};
