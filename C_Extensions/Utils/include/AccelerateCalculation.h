#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <cstdint>
#include <pybind11/pybind11.h>
#include <vector>

namespace py = pybind11;

void C_ImageUpdateBuffer1D(sf::Texture &img, py::buffer buffer);
void C_ImageUpdateBuffer3D(sf::Texture &img, py::buffer buffer);

void ApplyACBinding(py::module &m);
