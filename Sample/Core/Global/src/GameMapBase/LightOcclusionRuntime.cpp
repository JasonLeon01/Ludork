#include "LightOcclusionRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ludork::global::game_map_impl {

CellBounds cellBounds(const sf::Vector2i& origin, const sf::Vector2u& size) {
    return {origin.x, origin.y, static_cast<std::int64_t>(origin.x) + size.x,
            static_cast<std::int64_t>(origin.y) + size.y};
}

CellBounds cellBounds(const sf::IntRect& rect) {
    return {rect.position.x, rect.position.y,
            static_cast<std::int64_t>(rect.position.x) + rect.size.x,
            static_cast<std::int64_t>(rect.position.y) + rect.size.y};
}

CellBounds intersection(const CellBounds& left, const CellBounds& right) {
    return {std::max(left.left, right.left), std::max(left.top, right.top),
            std::min(left.right, right.right),
            std::min(left.bottom, right.bottom)};
}

bool isEmpty(const CellBounds& bounds) {
    return bounds.left >= bounds.right || bounds.top >= bounds.bottom;
}

bool actorIntersectsLight(const sf::FloatRect& bounds, const Light& light) {
    return bounds.position.x <= light.position.x + light.radius &&
           bounds.position.x + bounds.size.x >=
               light.position.x - light.radius &&
           bounds.position.y <= light.position.y + light.radius &&
           bounds.position.y + bounds.size.y >= light.position.y - light.radius;
}

sf::FloatRect enclosingRect(const sf::FloatRect& left,
                            const sf::FloatRect& right) {
    const float minimumX = std::min(left.position.x, right.position.x);
    const float minimumY = std::min(left.position.y, right.position.y);
    const float maximumX = std::max(left.position.x + left.size.x,
                                    right.position.x + right.size.x);
    const float maximumY = std::max(left.position.y + left.size.y,
                                    right.position.y + right.size.y);
    return {{minimumX, minimumY}, {maximumX - minimumX, maximumY - minimumY}};
}

std::optional<sf::FloatRect> dynamicMaskRect(
    const Light& light, const std::optional<sf::FloatRect>& actorBounds,
    float padding) {
    if (!actorBounds.has_value()) {
        return std::nullopt;
    }
    const float minimumX =
        std::max(std::floor(light.position.x - light.radius) - padding,
                 std::floor(actorBounds->position.x) - padding);
    const float minimumY =
        std::max(std::floor(light.position.y - light.radius) - padding,
                 std::floor(actorBounds->position.y) - padding);
    const float maximumX = std::min(
        std::ceil(light.position.x + light.radius) + padding,
        std::ceil(actorBounds->position.x + actorBounds->size.x) + padding);
    const float maximumY = std::min(
        std::ceil(light.position.y + light.radius) + padding,
        std::ceil(actorBounds->position.y + actorBounds->size.y) + padding);
    return sf::FloatRect({minimumX, minimumY},
                         {std::max(1.0f, maximumX - minimumX),
                          std::max(1.0f, maximumY - minimumY)});
}

bool hasStaticOccupancy(const std::vector<std::size_t>& prefix,
                        const sf::Vector2i& origin, const sf::Vector2u& size,
                        const Light& light, int cellSize) {
    if (prefix.empty() || size.x == 0 || size.y == 0 || light.radius <= 0.0f ||
        !std::isfinite(light.position.x) || !std::isfinite(light.position.y) ||
        !std::isfinite(light.radius)) {
        return false;
    }
    const double scale = static_cast<double>(cellSize);
    const double minimumXValue =
        std::floor((static_cast<double>(light.position.x) - light.radius) /
                   scale) -
        1.0;
    const double minimumYValue =
        std::floor((static_cast<double>(light.position.y) - light.radius) /
                   scale) -
        1.0;
    const double maximumXValue =
        std::floor((static_cast<double>(light.position.x) + light.radius) /
                   scale) +
        1.0;
    const double maximumYValue =
        std::floor((static_cast<double>(light.position.y) + light.radius) /
                   scale) +
        1.0;
    constexpr double MinimumCoordinate = -4611686018427387904.0;
    constexpr double MaximumCoordinate = 4611686018427387904.0;
    if (minimumXValue < MinimumCoordinate ||
        minimumYValue < MinimumCoordinate ||
        maximumXValue > MaximumCoordinate ||
        maximumYValue > MaximumCoordinate) {
        return false;
    }
    std::int64_t minimumX = static_cast<std::int64_t>(minimumXValue);
    std::int64_t minimumY = static_cast<std::int64_t>(minimumYValue);
    std::int64_t maximumX = static_cast<std::int64_t>(maximumXValue);
    std::int64_t maximumY = static_cast<std::int64_t>(maximumYValue);
    const std::int64_t occupancyRight =
        static_cast<std::int64_t>(origin.x) + size.x;
    const std::int64_t occupancyBottom =
        static_cast<std::int64_t>(origin.y) + size.y;
    minimumX = std::max(minimumX, static_cast<std::int64_t>(origin.x));
    minimumY = std::max(minimumY, static_cast<std::int64_t>(origin.y));
    maximumX = std::min(maximumX, occupancyRight - 1);
    maximumY = std::min(maximumY, occupancyBottom - 1);
    if (minimumX > maximumX || minimumY > maximumY) {
        return false;
    }

    const std::size_t firstX = static_cast<std::size_t>(minimumX - origin.x);
    const std::size_t firstY = static_cast<std::size_t>(minimumY - origin.y);
    const std::size_t lastX = static_cast<std::size_t>(maximumX - origin.x);
    const std::size_t lastY = static_cast<std::size_t>(maximumY - origin.y);
    const std::size_t prefixWidth = size.x + 1;
    const std::size_t topLeft = firstY * prefixWidth + firstX;
    const std::size_t topRight = firstY * prefixWidth + lastX + 1;
    const std::size_t bottomLeft = (lastY + 1) * prefixWidth + firstX;
    const std::size_t bottomRight = (lastY + 1) * prefixWidth + lastX + 1;
    const std::size_t included = prefix[bottomRight] + prefix[topLeft];
    const std::size_t excluded = prefix[topRight] + prefix[bottomLeft];
    return included - excluded > 0;
}

DynamicOccupancyResult rebuildDynamicOccupancy(
    const sf::FloatRect& maskRect,
    const std::vector<sf::FloatRect>& occluderBounds,
    std::shared_ptr<sf::Texture> texture, float cellSize) {
    const std::int64_t originX = static_cast<std::int64_t>(
        std::floor(static_cast<double>(maskRect.position.x) / cellSize));
    const std::int64_t originY = static_cast<std::int64_t>(
        std::floor(static_cast<double>(maskRect.position.y) / cellSize));
    const std::int64_t endX = static_cast<std::int64_t>(std::ceil(
        static_cast<double>(maskRect.position.x + maskRect.size.x) / cellSize));
    const std::int64_t endY = static_cast<std::int64_t>(std::ceil(
        static_cast<double>(maskRect.position.y + maskRect.size.y) / cellSize));
    const sf::Vector2u size(
        static_cast<unsigned int>(std::max<std::int64_t>(1, endX - originX)),
        static_cast<unsigned int>(std::max<std::int64_t>(1, endY - originY)));
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(size.x) * size.y * 4, 0);
    for (const sf::FloatRect& bounds : occluderBounds) {
        const std::int64_t left = std::max(
            originX, static_cast<std::int64_t>(std::floor(
                         static_cast<double>(bounds.position.x) / cellSize)));
        const std::int64_t top = std::max(
            originY, static_cast<std::int64_t>(std::floor(
                         static_cast<double>(bounds.position.y) / cellSize)));
        const std::int64_t right = std::min(
            endX, static_cast<std::int64_t>(std::ceil(
                      static_cast<double>(bounds.position.x + bounds.size.x) /
                      cellSize)));
        const std::int64_t bottom = std::min(
            endY, static_cast<std::int64_t>(std::ceil(
                      static_cast<double>(bounds.position.y + bounds.size.y) /
                      cellSize)));
        for (std::int64_t worldY = top; worldY < bottom; ++worldY) {
            const std::size_t textureY =
                size.y - static_cast<std::size_t>(worldY - originY) - 1;
            for (std::int64_t worldX = left; worldX < right; ++worldX) {
                const std::size_t textureX =
                    static_cast<std::size_t>(worldX - originX);
                const std::size_t pixelIndex =
                    (textureY * size.x + textureX) * 4;
                pixels[pixelIndex] = 255;
                pixels[pixelIndex + 1] = 255;
                pixels[pixelIndex + 2] = 255;
                pixels[pixelIndex + 3] = 255;
            }
        }
    }
    if (!texture) {
        texture = std::make_shared<sf::Texture>();
    }
    if (texture->getSize() != size && !texture->resize(size)) {
        throw std::runtime_error(
            "Failed to resize the dynamic light occupancy texture");
    }
    texture->update(pixels.data());
    texture->setSmooth(false);
    return {std::move(texture),
            {static_cast<float>(originX), static_cast<float>(originY)},
            {static_cast<float>(size.x), static_cast<float>(size.y)}};
}

}  // namespace ludork::global::game_map_impl
