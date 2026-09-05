#include "VisualRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace ludork::engine::drop_box_impl {

sf::Vector2f collapsedTextPosition(const sf::FloatRect& bounds,
                                   const sf::Vector2f& collapsedSize,
                                   float horizontalInset) {
    return {horizontalInset - bounds.position.x,
            (collapsedSize.y - bounds.size.y) * 0.5f - bounds.position.y};
}

sf::Vector2f itemTextPosition(const sf::FloatRect& bounds,
                              const sf::Vector2f& collapsedSize, int index,
                              float rowHeight) {
    return {(collapsedSize.x - bounds.size.x) * 0.5f - bounds.position.x,
            rowHeight * static_cast<float>(index) +
                (rowHeight - bounds.size.y) * 0.5f - bounds.position.y};
}

sf::Vector2u popupTextureSize(float width, float height, float scale) {
    return {
        static_cast<unsigned int>(std::max(0L, std::lround(width * scale))),
        static_cast<unsigned int>(std::max(0L, std::lround(height * scale)))};
}

bool selectionIntersectsViewport(int index, float rowHeight, float scrollOffset,
                                 float viewportHeight) {
    const float selectionTop = rowHeight * static_cast<float>(index);
    const float viewportBottom = scrollOffset + viewportHeight;
    return selectionTop < viewportBottom &&
           selectionTop + rowHeight > scrollOffset;
}

std::pair<int, int> visibleItemRange(std::size_t itemCount, float rowHeight,
                                     float scrollOffset, float viewportHeight) {
    const float viewportBottom = scrollOffset + viewportHeight;
    const int first =
        std::max(0, static_cast<int>(std::floor(scrollOffset / rowHeight)));
    const int last =
        std::min(static_cast<int>(itemCount),
                 static_cast<int>(std::ceil(viewportBottom / rowHeight)));
    return {first, last};
}

}  // namespace ludork::engine::drop_box_impl
