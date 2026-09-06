#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

namespace ludork::engine::actor_impl {

struct SpatialImpl {
    sf::Vector2f position;
    sf::Vector2i mapPosition;
    sf::FloatRect globalBounds;
    std::vector<sf::Vector2i> occupiedCells;

    void syncBounds(const sf::FloatRect& bounds, int cellSize);
    std::vector<sf::Vector2i> occupiedCellsAtMapPosition(
        const sf::Vector2i& target, int cellSize) const;
    std::vector<sf::Vector2i> computeOccupiedCells(const sf::FloatRect& bounds,
                                                   int cellSize) const;
};

int roundHalfToEven(float value);
sf::Vector2i mapPosition(const sf::Vector2f& position, int cellSize);
std::vector<sf::Vector2i> occupiedCells(const sf::FloatRect& bounds,
                                        const sf::Vector2i& fallback,
                                        int cellSize);

}  // namespace ludork::engine::actor_impl
