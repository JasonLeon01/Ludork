#pragma once

#include <UI/UiInputEventArguments.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>

namespace ludork::engine::ui_interaction {

std::optional<sf::Vector2f> pointerPosition(
    const UiInputEventArguments& arguments);
std::optional<sf::Mouse::Button> pointerMouseButton(
    const UiInputEventArguments& arguments);
std::optional<int> pointerButtonIndex(const UiInputEventArguments& arguments);

}  // namespace ludork::engine::ui_interaction
