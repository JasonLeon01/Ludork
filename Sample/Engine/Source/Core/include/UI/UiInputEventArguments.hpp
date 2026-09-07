#pragma once

#include <EngineRuntimeApi.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>
#include <optional>

BIND_CLASS(copyable = true, table_init = true)
struct LUDORK_ENGINE_API UiInputEventArguments {
    BIND_PROPERTY()
    std::optional<sf::Vector2f> position;

    BIND_PROPERTY()
    std::optional<sf::Mouse::Button> button;

    BIND_PROPERTY()
    std::optional<float> delta;
};
