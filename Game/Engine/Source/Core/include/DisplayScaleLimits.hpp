#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API DisplayScaleLimits {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const float MINIMUM_RENDER_SCALE;

    BIND_CLASS_PROPERTY(readonly = true)
    static const double MINIMUM_INVERTIBLE_SCALE;
};
