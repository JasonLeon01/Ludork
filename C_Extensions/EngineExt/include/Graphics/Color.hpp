#pragma once

#include <pybind11/pybind11.h>

#include <SFML/Graphics/Color.hpp>
#include <string>


namespace py = pybind11;

// BIND_FUNCTION
// Convert a hexadecimal colour string to an SFML Color.
// Supports #RRGGBB, $RRGGBB, 0xRRGGBB, and RRGGBBAA formats.
sf::Color C_HexColor(const std::string &value, int alpha = 255);

// BIND_FUNCTION
// Convert an integer colour value to an SFML Color.
sf::Color C_HexColor(int value, int alpha = 255);

void ApplyColorBinding(py::module &m);
