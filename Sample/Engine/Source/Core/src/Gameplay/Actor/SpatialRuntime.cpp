#include "SpatialRuntime.hpp"

#include <cmath>

namespace ludork::engine::actor_impl {

namespace {

bool boundsIntersectCell(const sf::Vector2f& boundsPosition,
                         const sf::Vector2f& boundsSize, float cellX,
                         float cellY, float cellSize) {
    const float boundsRight = boundsPosition.x + boundsSize.x;
    const float boundsBottom = boundsPosition.y + boundsSize.y;
    const float cellRight = cellX + cellSize;
    const float cellBottom = cellY + cellSize;
    return boundsPosition.x < cellRight && boundsRight > cellX &&
           boundsPosition.y < cellBottom && boundsBottom > cellY;
}

}  // namespace

int roundHalfToEven(float value) {
    const float lower = std::floor(value);
    const float fraction = value - lower;
    if (fraction < 0.5f) {
        return static_cast<int>(lower);
    }
    if (fraction > 0.5f) {
        return static_cast<int>(lower + 1.0f);
    }
    const int lowerInteger = static_cast<int>(lower);
    return lowerInteger % 2 == 0 ? lowerInteger : lowerInteger + 1;
}

sf::Vector2i mapPosition(const sf::Vector2f& position, int cellSize) {
    const float size = static_cast<float>(cellSize);
    return {static_cast<int>(position.x / size + 0.5f),
            static_cast<int>(position.y / size + 0.5f)};
}

std::vector<sf::Vector2i> occupiedCells(const sf::FloatRect& bounds,
                                        const sf::Vector2i& fallback,
                                        int cellSize) {
    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
        return {fallback};
    }
    const float size = static_cast<float>(cellSize);
    const int minX = static_cast<int>(std::floor(bounds.position.x / size));
    const int minY = static_cast<int>(std::floor(bounds.position.y / size));
    const int maxX = static_cast<int>(
        std::floor((bounds.position.x + bounds.size.x - 1e-9f) / size));
    const int maxY = static_cast<int>(
        std::floor((bounds.position.y + bounds.size.y - 1e-9f) / size));
    std::vector<sf::Vector2i> cells;
    for (int cellY = minY; cellY <= maxY; ++cellY) {
        for (int cellX = minX; cellX <= maxX; ++cellX) {
            if (boundsIntersectCell(bounds.position, bounds.size,
                                    static_cast<float>(cellX) * size,
                                    static_cast<float>(cellY) * size, size)) {
                cells.emplace_back(cellX, cellY);
            }
        }
    }
    return cells.empty() ? std::vector<sf::Vector2i>{fallback} : cells;
}

}  // namespace ludork::engine::actor_impl
