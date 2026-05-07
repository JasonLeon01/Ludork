#pragma once

#include <BindAnnotations.hpp>

#include <pybind11/pybind11.h>

#include <SFML/Graphics/Color.hpp>
#include <string>


namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Convert a hexadecimal color string to `sf::Color`
///
/// Supported input forms are `#RRGGBB`, `$RRGGBB`, `0xRRGGBB`
/// and `RRGGBBAA`.
///
/// - \param value Hexadecimal color string
/// - \param alpha Alpha value used when the string does not contain alpha
///
/// - \return Parsed color value
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
sf::Color C_HexColor(const std::string &value, int alpha = 255);

////////////////////////////////////////////////////////////
/// \brief Convert an integer color value to `sf::Color`
///
/// The low 24 bits are interpreted as RGB. The high 8 bits
/// are used as alpha when non-zero, otherwise `alpha` is used.
///
/// - \param value Packed integer color value
/// - \param alpha Fallback alpha value
///
/// - \return Parsed color value
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
sf::Color C_HexColor(int value, int alpha = 255);

////////////////////////////////////////////////////////////
/// \brief Register color helpers to a Python module
///
/// - \param m Target Python module
///
////////////////////////////////////////////////////////////
void ApplyColorBinding(py::module &m);
