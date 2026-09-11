#pragma once

#include <Curve.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LUDORK_ENGINE_API EmitterCurves {
    EmitterCurves();

    BIND_PROPERTY()
    Curve::CurveData speed;

    BIND_PROPERTY()
    Curve::CurveData sizeX;

    BIND_PROPERTY()
    Curve::CurveData sizeY;

    BIND_PROPERTY()
    Curve::CurveData rotation;

    BIND_PROPERTY()
    Curve::CurveData red;

    BIND_PROPERTY()
    Curve::CurveData green;

    BIND_PROPERTY()
    Curve::CurveData blue;

    BIND_PROPERTY()
    Curve::CurveData alpha;
};
