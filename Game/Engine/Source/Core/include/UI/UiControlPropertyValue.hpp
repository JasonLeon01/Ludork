#pragma once

#include <UI/Image.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using UiControlPropertyValue =
    std::variant<std::monostate, Image::DrawAs, bool, double, std::int64_t,
                 std::string, sf::Vector2f, sf::Vector2u, sf::IntRect,
                 sf::Color, std::vector<std::string>>;
using UiControlProperties =
    std::unordered_map<std::string, UiControlPropertyValue>;
