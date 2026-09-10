#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct MapFogSettings {
    BIND_PROPERTY()
    std::string fog;

    BIND_PROPERTY()
    float fogPower = 0.0f;

    BIND_PROPERTY()
    float fogOx = 0.0f;

    BIND_PROPERTY()
    float fogOy = 0.0f;

    BIND_PROPERTY()
    float fogDistort = 0.0f;
};
