#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace ludork::global::system_display_impl {

bool viewsEqual(const sf::View& left, const sf::View& right);
float windowFitScale(const sf::Vector2u& surfaceSize,
                     const sf::Vector2u& gameSize);
float effectiveRenderScale(float surfaceFitScale, float maximumRenderScale);
sf::Vector2u scaledSize(const sf::Vector2u& gameSize, float scale);

}  // namespace ludork::global::system_display_impl
