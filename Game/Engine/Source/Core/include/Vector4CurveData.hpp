#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Vector4CurveKey.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct Vector4CurveData {
    BIND_PROPERTY()
    std::string type = "vector4Curve";

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::array<float, 4> defaultValue = {0.0f, 0.0f, 0.0f, 0.0f};

    BIND_PROPERTY()
    std::string preInfinity = "constant";

    BIND_PROPERTY()
    std::string postInfinity = "constant";

    BIND_PROPERTY()
    std::vector<Vector4CurveKey> keys;
};
