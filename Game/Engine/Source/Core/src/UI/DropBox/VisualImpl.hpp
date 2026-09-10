#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <utility>

namespace ludork::engine::drop_box_impl {

[[nodiscard]] sf::Vector2f collapsedTextPosition(
    const sf::FloatRect& bounds, const sf::Vector2f& collapsedSize,
    float horizontalInset);
[[nodiscard]] sf::Vector2f itemTextPosition(const sf::FloatRect& bounds,
                                            const sf::Vector2f& collapsedSize,
                                            int index, float rowHeight);
[[nodiscard]] sf::Vector2u popupTextureSize(float width, float height,
                                            float scale);
[[nodiscard]] bool selectionIntersectsViewport(int index, float rowHeight,
                                               float scrollOffset,
                                               float viewportHeight);
[[nodiscard]] std::pair<int, int> visibleItemRange(std::size_t itemCount,
                                                   float rowHeight,
                                                   float scrollOffset,
                                                   float viewportHeight);

}  // namespace ludork::engine::drop_box_impl
