#pragma once

#include <UI/UiInputEventArguments.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>

namespace ludork::engine::functional_base_impl {

void validateTouchHitBounds(const std::optional<sf::FloatRect>& bounds);
UiInputEventArguments pointerArguments(const sf::Vector2f& position);
UiInputEventArguments mouseButtonArguments(const sf::Vector2f& position,
                                           sf::Mouse::Button button);
UiInputEventArguments mouseWheelArguments(const sf::Vector2f& position,
                                          float delta);
bool pointerStateIsEmpty(bool hovered, bool pressed, bool hasPointerSource);

}  // namespace ludork::engine::functional_base_impl
