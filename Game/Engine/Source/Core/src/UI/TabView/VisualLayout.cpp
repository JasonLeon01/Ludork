#include "VisualLayout.hpp"

#include "NavigationImpl.hpp"

namespace ludork::engine::tab_view_impl {

float slotWidth(float totalWidth, float hintSize, std::size_t itemCount) {
    return contentWidth(totalWidth, hintSize) / static_cast<float>(itemCount);
}

sf::Vector2f labelPosition(const sf::FloatRect& bounds,
                           const sf::Vector2f& size, int index,
                           std::size_t itemCount, float hintSize) {
    const float width = slotWidth(size.x, hintSize, itemCount);
    return {hintSize + width * (static_cast<float>(index) + 0.5f) -
                bounds.position.x - bounds.size.x * 0.5f,
            size.y * 0.5f - bounds.position.y - bounds.size.y * 0.5f};
}

sf::Vector2f hintPosition(const sf::Vector2f& size, bool left, float hintSize) {
    const float x = left ? 0.0f : size.x - hintSize;
    const float y = (size.y - hintSize) * 0.5f;
    return {x, y};
}

sf::Vector2f selectionPosition(float totalWidth, std::size_t itemCount,
                               int selectedIndex, float hintSize) {
    return {hintSize + slotWidth(totalWidth, hintSize, itemCount) *
                           static_cast<float>(selectedIndex),
            0.0f};
}

}  // namespace ludork::engine::tab_view_impl
