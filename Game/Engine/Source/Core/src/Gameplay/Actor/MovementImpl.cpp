#include "MovementImpl.hpp"

#include <algorithm>
#include <stdexcept>

namespace ludork::engine::actor_impl {

namespace {

int directionComponent(int value) {
    return value > 0 ? 1 : value < 0 ? -1 : 0;
}

}  // namespace

bool MovementImpl::isMoving() const {
    return moving || realSpeed > 0.0f || inRoute;
}

void MovementImpl::setRoute(
    const std::optional<std::vector<sf::Vector2i>>& nextRoute) {
    route = nextRoute;
    inRoute = route.has_value() && !route->empty();
}

std::optional<sf::Vector2i> MovementImpl::takeNextRouteStep() {
    if (!inRoute) {
        return std::nullopt;
    }
    if (!route.has_value() || route->empty()) {
        inRoute = false;
        return std::nullopt;
    }
    const sf::Vector2i step = route->front();
    route->erase(route->begin());
    return step;
}

void MovementImpl::cancelRoute() {
    inRoute = false;
    route = std::vector<sf::Vector2i>{};
}

void MovementImpl::stop() {
    moving = false;
    inRoute = false;
    route.reset();
    departure.reset();
    destination.reset();
    moveOriginMapPosition.reset();
    realSpeed = 0.0f;
}

void MovementImpl::arrive() {
    moving = false;
    departure.reset();
    destination.reset();
    moveOriginMapPosition.reset();
}

void MovementImpl::recordRealSpeed(float distance, float fixedDelta) {
    realSpeed =
        fixedDelta <= 0.0f || distance <= 0.001f ? 0.0f : distance / fixedDelta;
}

void MovementImpl::setDestination(const sf::Vector2i& offset, int cellSize) {
    destination =
        *departure + sf::Vector2f(static_cast<float>(offset.x * cellSize),
                                  static_cast<float>(offset.y * cellSize));
}

std::optional<sf::Vector2f> MovementImpl::velocity(float speed,
                                                   float speedRate) const {
    return movementVelocity(departure, destination, speed, speedRate);
}

sf::Vector2i normaliseDirection(const sf::Vector2i& direction) {
    return {directionComponent(direction.x), directionComponent(direction.y)};
}

std::optional<sf::Vector2f> movementVelocity(
    const std::optional<sf::Vector2f>& departure,
    const std::optional<sf::Vector2f>& destination, float speed,
    float speedRate) {
    if (!departure.has_value() || !destination.has_value()) {
        return std::nullopt;
    }
    const float actualSpeed = speed * speedRate;
    if (actualSpeed == 0.0f) {
        throw std::runtime_error("attempt to divide by zero");
    }
    const sf::Vector2f distance = *destination - *departure;
    return distance / (distance.length() / actualSpeed);
}

MovementAdvance advanceMovement(const sf::Vector2f& position,
                                const sf::Vector2f& destination,
                                const sf::Vector2f& velocity, float deltaTime) {
    const float remainingDistance = (destination - position).length();
    const float actualSpeed = velocity.length();
    if (actualSpeed <= 0.0f) {
        return {position, 0.0f, false};
    }
    const float completionTime = remainingDistance / actualSpeed;
    if (completionTime > deltaTime) {
        return {position + velocity * deltaTime, 0.0f, false};
    }
    return {destination, std::max(0.0f, deltaTime - completionTime), true};
}

sf::Vector2u snappedMapPosition(const sf::Vector2f& position, int cellSize) {
    const float size = static_cast<float>(cellSize);
    return {static_cast<unsigned int>(position.x / size + 0.5f),
            static_cast<unsigned int>(position.y / size + 0.5f)};
}

}  // namespace ludork::engine::actor_impl
