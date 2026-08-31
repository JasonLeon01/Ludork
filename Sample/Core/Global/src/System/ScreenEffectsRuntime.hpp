#pragma once

#include <SFML/Graphics/Glsl.hpp>

namespace ludork::global::system_screen_effects_impl {

sf::Glsl::Vec4 makeToneColour(float red, float green, float blue, float gray);
sf::Glsl::Vec4 interpolateTone(const sf::Glsl::Vec4& start,
                               const sf::Glsl::Vec4& target, float ratio);
bool isNeutralTone(const sf::Glsl::Vec4& colour);

}  // namespace ludork::global::system_screen_effects_impl
