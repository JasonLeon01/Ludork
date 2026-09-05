#include "NavigationRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace ludork::engine::tab_view_impl {

sf::Vector2f normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::max(0.0f, size.x) : 0.0f,
        std::isfinite(size.y) ? std::max(0.0f, size.y) : 0.0f,
    };
}

int clampedIndex(int index, std::size_t count) {
    return std::clamp(index, 0, static_cast<int>(count) - 1);
}

std::optional<int> tabIndexAt(const sf::Vector2f& size,
                              const sf::Vector2f& localPosition,
                              std::size_t itemCount, float hintSize) {
    const float right = size.x - hintSize;
    if (localPosition.x < hintSize || localPosition.x >= right ||
        localPosition.y < 0.0f || localPosition.y >= size.y ||
        right <= hintSize) {
        return std::nullopt;
    }
    const float slotWidth =
        contentWidth(size.x, hintSize) / static_cast<float>(itemCount);
    if (slotWidth <= 0.0f) {
        return std::nullopt;
    }
    const int index =
        static_cast<int>((localPosition.x - hintSize) / slotWidth);
    return std::clamp(index, 0, static_cast<int>(itemCount) - 1);
}

float contentWidth(float width, float hintSize) {
    return std::max(0.0f, width - hintSize * 2.0f);
}

}  // namespace ludork::engine::tab_view_impl
