#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>

namespace ludork::engine::ui_interaction {

std::optional<double> numericValue(RuntimeValueView value);
std::optional<sf::Vector2f> pointerPosition(const RuntimeValue::Map& arguments);
std::optional<sf::Mouse::Button> pointerMouseButton(
    const RuntimeValue::Map& arguments);
std::optional<int> pointerButtonIndex(const RuntimeValue::Map& arguments);

}  // namespace ludork::engine::ui_interaction
