#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

class Vector4Curve;

BIND_CLASS(copyable = true, table_init = true)
struct TextGradientConfig {
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string HORIZONTAL;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string VERTICAL;

    BIND_PROPERTY()
    bool enabled = false;

    BIND_PROPERTY(default = "vertical")
    std::string direction = VERTICAL;

    BIND_PROPERTY()
    std::shared_ptr<Vector4Curve> curve;
};
