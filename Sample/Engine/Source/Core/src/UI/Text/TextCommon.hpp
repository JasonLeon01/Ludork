#pragma once

#include <UI/Text.hpp>

#include <SFML/System/String.hpp>

#include <string>

namespace ludork::engine::text_detail {

sf::String toSfString(const std::string& value);
unsigned int scaledCharacterSize(unsigned int characterSize);
void validateGradient(const TextGradientConfig& gradient);
sf::Color modulateColour(const sf::Color& baseColour,
                         const sf::Color& factorColour);

}  // namespace ludork::engine::text_detail
