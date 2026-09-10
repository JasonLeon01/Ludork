#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct TextGlowConfig {
    BIND_PROPERTY()
    bool enabled = false;

    BIND_PROPERTY()
    sf::Color color = sf::Color::Transparent;

    BIND_PROPERTY()
    float radius = 0.0f;

    BIND_PROPERTY()
    float intensity = 0.0f;
};
