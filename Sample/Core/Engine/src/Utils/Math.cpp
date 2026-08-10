#include <Utils/Math.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace {

std::int64_t checkedInteger(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Expected a finite numeric value");
    }
    const double minimum =
        static_cast<double>(std::numeric_limits<std::int64_t>::min());
    const double maximum =
        static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (value < minimum || value >= maximum) {
        throw std::out_of_range("Numeric value is outside the integer range");
    }
    return static_cast<std::int64_t>(value);
}

}  // namespace

std::int64_t roundNumber(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Expected a finite numeric value");
    }
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) {
        return checkedInteger(lower);
    }
    if (fraction > 0.5) {
        return checkedInteger(lower + 1.0);
    }
    const double rounded = std::fmod(lower, 2.0) == 0.0 ? lower : lower + 1.0;
    return checkedInteger(rounded);
}

std::int64_t toInteger(const RuntimeValue& value) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return *integer;
    }
    if (const double* number = value.getIf<double>()) {
        return checkedInteger(std::trunc(*number));
    }
    if (const std::string* text = value.getIf<std::string>()) {
        const char* begin = text->c_str();
        char* end = nullptr;
        const double number = std::strtod(begin, &end);
        if (end == begin) {
            throw std::invalid_argument("Expected a numeric string");
        }
        while (*end != '\0' &&
               std::isspace(static_cast<unsigned char>(*end)) != 0) {
            ++end;
        }
        if (*end != '\0') {
            throw std::invalid_argument("Expected a numeric string");
        }
        return checkedInteger(std::trunc(number));
    }
    throw std::invalid_argument("Expected a number or numeric string");
}

bool isNearZero(double number, double epsilon) {
    return std::abs(number) < epsilon;
}

bool isVector2NearZero(const sf::Vector2f& value, float epsilon) {
    return isNearZero(value.x, epsilon) && isNearZero(value.y, epsilon);
}

bool isVector3NearZero(const sf::Vector3f& value, float epsilon) {
    return isNearZero(value.x, epsilon) && isNearZero(value.y, epsilon) &&
           isNearZero(value.z, epsilon);
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

double clampNumber(double value, double minimum, double maximum) {
    return std::clamp(value, minimum, maximum);
}

double lerpNumber(double from, double to, double alpha) {
    return from + (to - from) * alpha;
}

std::int64_t greatestCommonDivisor(std::int64_t left, std::int64_t right) {
    return std::gcd(left, right);
}

std::int64_t leastCommonMultiple(std::int64_t left, std::int64_t right) {
    return std::lcm(left, right);
}
