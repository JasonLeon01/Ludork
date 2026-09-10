#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct AutoSoundParams {
    BIND_PROPERTY()
    float volume = 100.0f;

    BIND_PROPERTY()
    float minDistance = 64.0f;

    BIND_PROPERTY()
    float attenuation = 1.0f;

    BIND_PROPERTY()
    bool loop = false;

    BIND_PROPERTY()
    float maxDistance = 0.0f;
};
