#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct Vector4CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 4> value = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::string interpolation = "linear";

    BIND_PROPERTY()
    std::array<float, 4> arriveTangent = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::array<float, 4> leaveTangent = {0.0f, 0.0f, 0.0f, 0.0f};
};
