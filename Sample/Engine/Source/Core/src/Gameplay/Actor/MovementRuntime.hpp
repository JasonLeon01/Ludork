#pragma once

#include <SFML/System/Vector2.hpp>

#include <optional>

namespace ludork::engine::actor_impl {

struct MovementAdvance {
    sf::Vector2f position;
    float remainingTime = 0.0f;
    bool completed = false;
};

sf::Vector2i normaliseDirection(const sf::Vector2i& direction);
std::optional<sf::Vector2f> movementVelocity(
    const std::optional<sf::Vector2f>& departure,
    const std::optional<sf::Vector2f>& destination, float speed,
    float speedRate);
MovementAdvance advanceMovement(const sf::Vector2f& position,
                                const sf::Vector2f& destination,
                                const sf::Vector2f& velocity, float deltaTime);
sf::Vector2u snappedMapPosition(const sf::Vector2f& position, int cellSize);

}  // namespace ludork::engine::actor_impl
