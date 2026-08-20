#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <vector>

namespace ludork::preview_host {

std::vector<std::uint8_t> bgraFromPremultipliedRgba(const sf::Image& image,
                                                    const sf::Vector2u& size);
std::vector<std::uint8_t> premultipliedBgraFromStraightRgba(
    const sf::Image& image, const sf::Vector2u& size);

}  // namespace ludork::preview_host
