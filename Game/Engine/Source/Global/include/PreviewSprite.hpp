#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct PreviewSprite {
    BIND_PROPERTY()
    std::string layer;
    BIND_PROPERTY()
    std::shared_ptr<sf::Texture> texture;
    BIND_PROPERTY()
    sf::IntRect rect;
    BIND_PROPERTY()
    sf::Vector2f position;
    BIND_PROPERTY()
    sf::Vector2i mapPosition;
    BIND_PROPERTY()
    sf::Vector2f translation;
    BIND_PROPERTY()
    float rotation = 0.0f;
    BIND_PROPERTY(default = {1.0, 1.0})
    sf::Vector2f scale{1.0f, 1.0f};
    BIND_PROPERTY()
    sf::Vector2f origin;
    BIND_PROPERTY()
    bool visible = true;
    BIND_PROPERTY()
    float hue = 0.0f;
    BIND_PROPERTY()
    std::string shaderPath;
    BIND_PROPERTY()
    int parentIndex = -1;
};
