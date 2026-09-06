#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct AnimationSoundEntry {
    BIND_PROPERTY()
    std::string asset;

    BIND_PROPERTY()
    int startFrame = 0;

    BIND_PROPERTY()
    int endFrame = 0;

    BIND_PROPERTY()
    std::optional<float> originalDuration;

    BIND_PROPERTY()
    std::optional<bool> stopAtEndFrame;
};
