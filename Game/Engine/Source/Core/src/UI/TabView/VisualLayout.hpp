#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>

namespace ludork::engine::tab_view_impl {

struct HintLayout {
    sf::Vector2f backgroundPosition;
    sf::Vector2f textPosition;
    float textScale = 1.0f;
};

[[nodiscard]] float slotWidth(float totalWidth, float hintSize,
                              std::size_t itemCount);
[[nodiscard]] sf::Vector2f labelPosition(const sf::FloatRect& bounds,
                                         const sf::Vector2f& size, int index,
                                         std::size_t itemCount, float hintSize);
[[nodiscard]] HintLayout hintLayout(const sf::FloatRect& bounds,
                                    const sf::Vector2f& size, bool left,
                                    float hintSize, float contentSize);
[[nodiscard]] sf::Vector2f selectionPosition(float totalWidth,
                                             std::size_t itemCount,
                                             int selectedIndex, float hintSize);

}  // namespace ludork::engine::tab_view_impl
