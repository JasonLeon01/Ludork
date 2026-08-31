#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

namespace ludork::engine::actor_impl {

int roundHalfToEven(float value);
sf::Vector2i mapPosition(const sf::Vector2f& position, int cellSize);
std::vector<sf::Vector2i> occupiedCells(const sf::FloatRect& bounds,
                                        const sf::Vector2i& fallback,
                                        int cellSize);

}  // namespace ludork::engine::actor_impl
