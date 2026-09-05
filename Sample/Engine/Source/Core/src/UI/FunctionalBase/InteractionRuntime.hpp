#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Mouse.hpp>

#include <optional>

namespace ludork::engine::functional_base_impl {

void validateTouchHitBounds(const std::optional<sf::FloatRect>& bounds);
RuntimeValue::Map pointerArguments(const sf::Vector2f& position);
RuntimeValue::Map mouseButtonArguments(const sf::Vector2f& position,
                                       sf::Mouse::Button button);
RuntimeValue::Map mouseWheelArguments(const sf::Vector2f& position,
                                      float delta);
bool pointerStateIsEmpty(bool hovered, bool pressed, bool hasPointerSource);

}  // namespace ludork::engine::functional_base_impl
