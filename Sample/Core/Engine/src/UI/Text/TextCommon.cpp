#include "TextCommon.hpp"

#include <EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace ludork::engine::text_detail {

sf::String toSfString(const std::string& value) {
    return sf::String::fromUtf8(value.begin(), value.end());
}

unsigned int scaledCharacterSize(unsigned int characterSize) {
    return static_cast<unsigned int>(
        std::max(1.0f, std::floor(static_cast<float>(characterSize) * Scale)));
}

void validateGradient(const TextGradientConfig& gradient) {
    if (gradient.direction != "vertical" &&
        gradient.direction != "horizontal") {
        throw std::invalid_argument(
            "Text gradient direction must be vertical or horizontal");
    }
}

sf::Color modulateColour(const sf::Color& baseColour,
                         const sf::Color& factorColour) {
    return {
        static_cast<std::uint8_t>(baseColour.r * factorColour.r / 255),
        static_cast<std::uint8_t>(baseColour.g * factorColour.g / 255),
        static_cast<std::uint8_t>(baseColour.b * factorColour.b / 255),
        static_cast<std::uint8_t>(baseColour.a * factorColour.a / 255),
    };
}

}  // namespace ludork::engine::text_detail
