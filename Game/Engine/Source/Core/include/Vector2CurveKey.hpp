#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct Vector2CurveKey {
    BIND_PROPERTY()
    float time = 0.0f;

    BIND_PROPERTY()
    std::array<float, 2> value = {0.0f, 0.0f};

    BIND_PROPERTY()
    std::string interpolation = "linear";

    BIND_PROPERTY()
    std::array<float, 2> arriveTangent = {0.0f, 0.0f};

    BIND_PROPERTY()
    std::array<float, 2> leaveTangent = {0.0f, 0.0f};
};
