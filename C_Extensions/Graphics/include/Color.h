#pragma once

#include <SFML/Graphics/Color.hpp>
#include <pybind11/pybind11.h>
#include <string>

sf::Color C_HexColor(const std::string &value, int alpha = 255);
sf::Color C_HexColor(int value, int alpha = 255);
