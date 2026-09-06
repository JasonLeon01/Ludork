#pragma once

#include <CoreMinimal.hpp>
#include <Light.hpp>

class Actor;

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LightOcclusionInput {
    BIND_PROPERTY(metadata = false)
    Light light;

    BIND_PROPERTY(metadata = false)
    std::shared_ptr<Actor> owner;
};
