#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct AnimationTimeTag {
    BIND_PROPERTY()
    std::string tag;

    BIND_PROPERTY()
    float time = 0.0f;
};
