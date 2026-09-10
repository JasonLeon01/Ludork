#include "InteractionImpl.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace ludork::engine::functional_base_impl {

void validateTouchHitBounds(const std::optional<sf::FloatRect>& bounds) {
    if (bounds.has_value() &&
        (!std::isfinite(bounds->position.x) ||
         !std::isfinite(bounds->position.y) || !std::isfinite(bounds->size.x) ||
         !std::isfinite(bounds->size.y) || bounds->size.x < 0.0f ||
         bounds->size.y < 0.0f)) {
        throw std::invalid_argument(
            "Touch hit bounds must be finite with non-negative size");
    }
}

UiInputEventArguments pointerArguments(const sf::Vector2f& position) {
    return {.position = position};
}

UiInputEventArguments mouseButtonArguments(const sf::Vector2f& position,
                                           sf::Mouse::Button button) {
    return {.position = position, .button = button};
}

UiInputEventArguments mouseWheelArguments(const sf::Vector2f& position,
                                          float delta) {
    return {.position = position, .delta = delta};
}

bool pointerStateIsEmpty(bool hovered, bool pressed, bool hasPointerSource) {
    return !hovered && !pressed && !hasPointerSource;
}

}  // namespace ludork::engine::functional_base_impl
