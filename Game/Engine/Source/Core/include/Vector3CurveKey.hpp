#pragma once

#include <CurveInterpolation.hpp>

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct Vector3CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 3> value = {0.0f, 0.0f, 0.0f};

    BIND_PROPERTY(default = "linear")
    std::string interpolation = CurveInterpolation::LINEAR;

    BIND_PROPERTY()
    std::array<float, 3> arriveTangent = {0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::array<float, 3> leaveTangent = {0.0f, 0.0f, 0.0f};
};
