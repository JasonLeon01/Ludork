#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>

BIND_FUNCTION(name = "Round")
LUDORK_ENGINE_API std::int64_t roundNumber(double value);

BIND_FUNCTION(name = "ToInteger")
std::int64_t toInteger(const RuntimeValue& value);

BIND_FUNCTION(name = "IsNearZero")
LUDORK_ENGINE_API bool isNearZero(double number, double epsilon = 0.1);

BIND_FUNCTION(name = "IsVector2NearZero")
bool isVector2NearZero(const sf::Vector2f& value, float epsilon = 0.1f);

BIND_FUNCTION(name = "IsVector3NearZero")
bool isVector3NearZero(const sf::Vector3f& value, float epsilon = 0.1f);

BIND_FUNCTION(name = "Vector2fRound")
sf::Vector2f vector2fRound(const sf::Vector2f& value);

BIND_FUNCTION(name = "Vector2fFloor")
sf::Vector2f vector2fFloor(const sf::Vector2f& value);

BIND_FUNCTION(name = "Vector2fCeil")
sf::Vector2f vector2fCeil(const sf::Vector2f& value);

BIND_FUNCTION(name = "ToVector2f")
sf::Vector2f toVector2f(const sf::Vector2i& value);

BIND_FUNCTION(name = "ToVector2f")
sf::Vector2f toVector2f(const sf::Vector2u& value);

BIND_FUNCTION(name = "ToVector2i")
sf::Vector2i toVector2i(const sf::Vector2f& value);

BIND_FUNCTION(name = "ToVector2i")
sf::Vector2i toVector2i(const sf::Vector2u& value);

BIND_FUNCTION(name = "ToVector2u")
sf::Vector2u toVector2u(const sf::Vector2f& value);

BIND_FUNCTION(name = "ToVector2u")
sf::Vector2u toVector2u(const sf::Vector2i& value);

BIND_FUNCTION(name = "ToVector3f")
sf::Vector3f toVector3f(const sf::Vector3i& value);

BIND_FUNCTION(name = "ToVector3i")
sf::Vector3i toVector3i(const sf::Vector3f& value);

BIND_FUNCTION(name = "ToIntRect")
sf::IntRect toIntRect(int x, int y, int width, int height);

BIND_FUNCTION(name = "ToFloatRect")
sf::FloatRect toFloatRect(float x, float y, float width, float height);

BIND_FUNCTION(name = "Clamp")
LUDORK_ENGINE_API double clampNumber(double value, double minimum,
                                     double maximum);

BIND_FUNCTION(name = "Lerp")
double lerpNumber(double from, double to, double alpha);

BIND_FUNCTION(name = "ManhattanDistance")
std::int64_t manhattanDistance(const sf::Vector2i& left,
                               const sf::Vector2i& right);

BIND_FUNCTION(name = "GCD")
std::int64_t greatestCommonDivisor(std::int64_t left, std::int64_t right);

BIND_FUNCTION(name = "LCM")
std::int64_t leastCommonMultiple(std::int64_t left, std::int64_t right);
