#include "PopupRuntime.hpp"

#include <algorithm>
#include <cmath>

namespace ludork::engine::drop_box_impl {

sf::FloatRect intersectRects(const sf::FloatRect& left,
                             const sf::FloatRect& right) {
    const float minX = std::max(left.position.x, right.position.x);
    const float minY = std::max(left.position.y, right.position.y);
    const float maxX = std::min(left.position.x + left.size.x,
                                right.position.x + right.size.x);
    const float maxY = std::min(left.position.y + left.size.y,
                                right.position.y + right.size.y);
    return {{minX, minY},
            {std::max(0.0f, maxX - minX), std::max(0.0f, maxY - minY)}};
}

PopupGeometry calculatePopupGeometry(const sf::Vector2f& collapsedSize,
                                     const sf::FloatRect& localConstraint,
                                     float naturalHeight, std::size_t itemCount,
                                     float scale, float borderHeight,
                                     float rowHeight) {
    PopupGeometry result;
    if (localConstraint.size.x <= 0.0f || localConstraint.size.y <= 0.0f) {
        return result;
    }
    const float constraintTop = localConstraint.position.y / scale;
    const float constraintBottom =
        (localConstraint.position.y + localConstraint.size.y) / scale;
    const float downwardTop = std::max(0.0f, constraintTop);
    const float upwardBottom = std::min(collapsedSize.y, constraintBottom);
    const float downwardSpace = std::max(0.0f, constraintBottom - downwardTop);
    const float upwardSpace = std::max(0.0f, upwardBottom - constraintTop);

    bool upward = false;
    if (naturalHeight <= downwardSpace) {
        result.positionY = downwardTop;
        result.height = naturalHeight;
    } else if (naturalHeight <= upwardSpace) {
        upward = true;
        result.height = naturalHeight;
    } else if (downwardSpace >= upwardSpace) {
        result.positionY = downwardTop;
        result.height = std::min(naturalHeight, downwardSpace);
    } else {
        upward = true;
        result.height = std::min(naturalHeight, upwardSpace);
    }
    result.height = std::floor(result.height);
    if (upward) {
        result.positionY = upwardBottom - result.height;
    }
    result.contentHeight = std::max(0.0f, result.height - borderHeight);
    const float itemContentHeight =
        rowHeight * static_cast<float>(std::max<std::size_t>(itemCount, 1U));
    result.maxScrollOffset =
        std::max(0.0f, itemContentHeight - result.contentHeight);
    return result;
}

float clampScrollOffset(float offset, float maximum) {
    return std::clamp(offset, 0.0f, maximum);
}

float advanceScrollOffset(float offset, float target, float deltaTime,
                          float response) {
    const float factor = 1.0f - std::exp(-response * std::max(0.0f, deltaTime));
    return offset + (target - offset) * factor;
}

}  // namespace ludork::engine::drop_box_impl
