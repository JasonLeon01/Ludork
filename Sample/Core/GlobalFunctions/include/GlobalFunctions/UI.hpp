#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics/Color.hpp>
#include <string>

BIND_FUNCTION_GROUP(name = "UI")

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
BIND_FUNCTION(name = "HexColor")
sf::Color hexColor(const std::string& value, int alpha = 255);

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
BIND_FUNCTION(name = "HexColor")
sf::Color hexColor(int value, int alpha = 255);

BIND_FUNCTION(name = "GetRosyBrown")
sf::Color getRosyBrown();

BIND_FUNCTION(name = "GetCopper")
sf::Color getCopper();

BIND_FUNCTION(name = "GetSage")
sf::Color getSage();

BIND_FUNCTION(name = "GetTeal")
sf::Color getTeal();

BIND_FUNCTION(name = "GetMutedPurple")
sf::Color getMutedPurple();

BIND_FUNCTION(name = "GetTaupe")
sf::Color getTaupe();

BIND_FUNCTION(name = "GetTerraCotta")
sf::Color getTerraCotta();

BIND_FUNCTION(name = "GetOchre")
sf::Color getOchre();

BIND_FUNCTION(name = "GetFernGreen")
sf::Color getFernGreen();

BIND_FUNCTION(name = "GetSteelBlue")
sf::Color getSteelBlue();

BIND_FUNCTION(name = "GetDimGrey")
sf::Color getDimGrey();

BIND_FUNCTION(name = "GetCharcoal")
sf::Color getCharcoal();
