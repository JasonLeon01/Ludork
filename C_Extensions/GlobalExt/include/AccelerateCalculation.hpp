#pragma once

#include <BindAnnotations.hpp>

#include <pybind11/pybind11.h>

#include <SFML/Graphics/Texture.hpp>
#include <cstdint>
#include <vector>


namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Update an `sf::Texture` using a flat byte buffer
///
/// - \param img Target texture
/// - \param buffer 1D `uint8` RGBA buffer
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
void C_ImageUpdateBuffer1D(sf::Texture &img, py::buffer buffer);

////////////////////////////////////////////////////////////
/// \brief Update an `sf::Texture` using a 3D RGBA buffer
///
/// - \param img Target texture
/// - \param buffer 3D `uint8` buffer with shape `[height, width, 4]`
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
void C_ImageUpdateBuffer3D(sf::Texture &img, py::buffer buffer);

////////////////////////////////////////////////////////////
/// \brief Register accelerated texture helpers to Python
///
/// - \param m Target Python module
///
////////////////////////////////////////////////////////////
void ApplyACBinding(py::module &m);
