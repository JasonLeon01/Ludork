#pragma once

#include <SFML/System/Vector2.hpp>

#include <optional>
#include <vector>

namespace ludork::engine::actor_impl {

struct MovementImpl {
    bool moving = false;
    bool inRoute = false;
    std::optional<std::vector<sf::Vector2i>> route =
        std::vector<sf::Vector2i>{};
    bool moveEnabled = true;
    std::optional<sf::Vector2f> departure;
    std::optional<sf::Vector2f> destination;
    std::optional<sf::Vector2i> moveOriginMapPosition;
    float realSpeed = 0.0f;

    bool isMoving() const;
    void setRoute(const std::optional<std::vector<sf::Vector2i>>& nextRoute);
    std::optional<sf::Vector2i> takeNextRouteStep();
    void cancelRoute();
    void stop();
    void arrive();
    void recordRealSpeed(float distance, float fixedDelta);
    void setDestination(const sf::Vector2i& offset, int cellSize);
    std::optional<sf::Vector2f> velocity(float speed, float speedRate) const;
};

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
