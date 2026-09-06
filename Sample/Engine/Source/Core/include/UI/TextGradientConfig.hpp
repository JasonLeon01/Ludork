#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

class Vector4Curve;

BIND_CLASS(copyable = true, table_init = true)
struct TextGradientConfig {
    BIND_PROPERTY()
    bool enabled = false;

    BIND_PROPERTY()
    std::string direction = "vertical";

    BIND_PROPERTY()
    std::shared_ptr<Vector4Curve> curve;
};
