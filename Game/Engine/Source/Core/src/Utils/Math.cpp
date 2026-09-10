#include <Utils/Math.hpp>
#include <Math.hpp>

#include <cmath>

bool isVector2NearZero(const sf::Vector2f& value, float epsilon) {
    return ludork::standard::math::isNearZero(value.x, epsilon) &&
           ludork::standard::math::isNearZero(value.y, epsilon);
}

bool isVector3NearZero(const sf::Vector3f& value, float epsilon) {
    return ludork::standard::math::isNearZero(value.x, epsilon) &&
           ludork::standard::math::isNearZero(value.y, epsilon) &&
           ludork::standard::math::isNearZero(value.z, epsilon);
}

sf::Vector2f vector2fRound(const sf::Vector2f& value) {
    return {std::nearbyint(value.x), std::nearbyint(value.y)};
}

sf::Vector2f vector2fFloor(const sf::Vector2f& value) {
    return {std::floor(value.x), std::floor(value.y)};
}

sf::Vector2f vector2fCeil(const sf::Vector2f& value) {
    return {std::ceil(value.x), std::ceil(value.y)};
}

sf::Vector2f toVector2f(const sf::Vector2i& value) {
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

sf::Vector2f toVector2f(const sf::Vector2u& value) {
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

sf::Vector2i toVector2i(const sf::Vector2f& value) {
    return {static_cast<int>(value.x), static_cast<int>(value.y)};
}

sf::Vector2i toVector2i(const sf::Vector2u& value) {
    return {static_cast<int>(value.x), static_cast<int>(value.y)};
}

sf::Vector2u toVector2u(const sf::Vector2f& value) {
    return {static_cast<unsigned int>(static_cast<int>(value.x)),
            static_cast<unsigned int>(static_cast<int>(value.y))};
}

sf::Vector2u toVector2u(const sf::Vector2i& value) {
    return {static_cast<unsigned int>(value.x),
            static_cast<unsigned int>(value.y)};
}

sf::Vector3f toVector3f(const sf::Vector3i& value) {
    return {static_cast<float>(value.x), static_cast<float>(value.y),
            static_cast<float>(value.z)};
}

sf::Vector3i toVector3i(const sf::Vector3f& value) {
    return {static_cast<int>(value.x), static_cast<int>(value.y),
            static_cast<int>(value.z)};
}

sf::IntRect toIntRect(int x, int y, int width, int height) {
    return {{x, y}, {width, height}};
}

sf::FloatRect toFloatRect(float x, float y, float width, float height) {
    return {{x, y}, {width, height}};
}

std::int64_t manhattanDistance(const sf::Vector2i& left,
                               const sf::Vector2i& right) {
    const std::int64_t horizontal = static_cast<std::int64_t>(left.x) - right.x;
    const std::int64_t vertical = static_cast<std::int64_t>(left.y) - right.y;
    return std::abs(horizontal) + std::abs(vertical);
}
