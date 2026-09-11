#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>

namespace ludork::engine::tab_view_impl {

[[nodiscard]] float slotWidth(float totalWidth, float hintSize,
                              std::size_t itemCount);
[[nodiscard]] sf::Vector2f labelPosition(const sf::FloatRect& bounds,
                                         const sf::Vector2f& size, int index,
                                         std::size_t itemCount, float hintSize);
[[nodiscard]] sf::Vector2f hintPosition(const sf::Vector2f& size, bool left,
                                        float hintSize);
[[nodiscard]] sf::Vector2f selectionPosition(float totalWidth,
                                             std::size_t itemCount,
                                             int selectedIndex, float hintSize);

}  // namespace ludork::engine::tab_view_impl
