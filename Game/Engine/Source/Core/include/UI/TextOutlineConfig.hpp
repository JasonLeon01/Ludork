#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true)
struct TextOutlineConfig {
    BIND_PROPERTY()
    sf::Color color = sf::Color::Black;

    BIND_PROPERTY()
    float thickness = 0.0f;
};
