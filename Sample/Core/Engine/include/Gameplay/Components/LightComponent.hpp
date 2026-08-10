#pragma once

#include <BindAnnotations.hpp>
#include <Gameplay/Components/Component.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

BIND_CLASS(table_init = true)
class LightComponent : public Component {
public:
    BIND_INIT()
    LightComponent() = default;

    BIND_PROPERTY(default = {255, 255, 255, 255})
    sf::Color lightColour = sf::Color::White;

    BIND_PROPERTY()
    float lightRadius = 16.0f;

    BIND_PROPERTY(default = {0.0, 0.0})
    sf::Vector2f lightOffset{0.0f, 0.0f};
};
