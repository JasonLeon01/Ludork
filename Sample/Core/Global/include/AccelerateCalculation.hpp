#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics/Texture.hpp>
#include <cstdint>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////
/// \brief Update an `sf::Texture` using a flat byte buffer
///
/// - \param img Target texture
/// - \param buffer 1D `uint8` RGBA buffer
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
void C_ImageUpdateBuffer1D(sf::Texture& img, const std::string& buffer);

////////////////////////////////////////////////////////////
/// \brief Update an `sf::Texture` using a 3D RGBA buffer
///
/// - \param img Target texture
/// - \param buffer 3D `uint8` buffer with shape `[height, width, 4]`
///
////////////////////////////////////////////////////////////
BIND_FUNCTION()
void C_ImageUpdateBuffer3D(sf::Texture& img, const std::string& buffer);
