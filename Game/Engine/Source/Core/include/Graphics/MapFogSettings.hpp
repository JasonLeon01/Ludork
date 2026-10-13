#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct LUDORK_ENGINE_API MapFogSettings {
    BIND_PROPERTY()
    bool enabled = false;
    BIND_PROPERTY(default = {255, 255, 255, 255})
    sf::Color colour = sf::Color::White;
    BIND_PROPERTY()
    float density = 0.5f;
    BIND_PROPERTY()
    float scale = 256.0f;
    BIND_PROPERTY(default = {15.0, 15.0})
    sf::Vector2f velocity{15.0f, 15.0f};
    BIND_PROPERTY()
    float turbulence = 0.5f;
    BIND_PROPERTY()
    int seed = 0;
};
