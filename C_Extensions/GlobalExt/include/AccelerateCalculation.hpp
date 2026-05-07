#pragma once

#include <pybind11/pybind11.h>

#include <SFML/Graphics/Texture.hpp>
#include <cstdint>
#include <vector>


namespace py = pybind11;

// BIND_FUNCTION
// Update a texture from a flat 1D uint8 buffer.
void C_ImageUpdateBuffer1D(sf::Texture &img, py::buffer buffer);

// BIND_FUNCTION
// Update a texture from a 3D [height, width, 4] uint8 buffer.
void C_ImageUpdateBuffer3D(sf::Texture &img, py::buffer buffer);

void ApplyACBinding(py::module &m);
