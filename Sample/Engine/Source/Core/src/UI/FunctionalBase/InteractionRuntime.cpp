#include "InteractionRuntime.hpp"

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

RuntimeValue::Map pointerArguments(const sf::Vector2f& position) {
    return {{"position",
             RuntimeValue(RuntimeValue::Map{{"x", RuntimeValue(position.x)},
                                            {"y", RuntimeValue(position.y)}})}};
}

RuntimeValue::Map mouseButtonArguments(const sf::Vector2f& position,
                                       sf::Mouse::Button button) {
    RuntimeValue::Map result = pointerArguments(position);
    result.emplace("button", RuntimeValue(static_cast<std::int64_t>(button)));
    return result;
}

RuntimeValue::Map mouseWheelArguments(const sf::Vector2f& position,
                                      float delta) {
    RuntimeValue::Map result = pointerArguments(position);
    result.emplace("delta", RuntimeValue(static_cast<double>(delta)));
    return result;
}

bool pointerStateIsEmpty(bool hovered, bool pressed, bool hasPointerSource) {
    return !hovered && !pressed && !hasPointerSource;
}

}  // namespace ludork::engine::functional_base_impl
