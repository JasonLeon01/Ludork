#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>

namespace ludork::engine::drop_box_impl {

struct PopupGeometry {
    float positionY = 0.0f;
    float height = 0.0f;
    float contentHeight = 0.0f;
    float maxScrollOffset = 0.0f;
};

sf::FloatRect intersectRects(const sf::FloatRect& left,
                             const sf::FloatRect& right);
PopupGeometry calculatePopupGeometry(const sf::Vector2f& collapsedSize,
                                     const sf::FloatRect& localConstraint,
                                     float naturalHeight, std::size_t itemCount,
                                     float scale, float borderHeight,
                                     float rowHeight);
float clampScrollOffset(float offset, float maximum);
float advanceScrollOffset(float offset, float target, float deltaTime,
                          float response);

}  // namespace ludork::engine::drop_box_impl
