#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace ludork::preview_host {

namespace preview_pixels {
inline constexpr unsigned int BytesPerPixel = 4;
inline constexpr char Format[] = "Bgra8888Premultiplied";
}  // namespace preview_pixels

std::vector<std::uint8_t> bgraFromPremultipliedRgba(const sf::Image& image,
                                                    const sf::Vector2u& size);
void bgraFromPremultipliedRgba(const sf::Image& image, const sf::Vector2u& size,
                               std::span<std::uint8_t> target);
std::vector<std::uint8_t> premultipliedBgraFromStraightRgba(
    const sf::Image& image, const sf::Vector2u& size);

}  // namespace ludork::preview_host
