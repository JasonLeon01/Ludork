#pragma once

#include <SFML/Graphics/Color.hpp>
#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

sf::Color C_HexColor(const std::string &value, int alpha = 255);
sf::Color C_HexColor(int value, int alpha = 255);

void ApplyColorBinding(py::module &m);
