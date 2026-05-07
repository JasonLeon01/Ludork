#pragma once

#include <BindAnnotations.hpp>

#include <pybind11/pybind11.h>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>


namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Utility for rendering nine-patch style rectangles
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class RectBase {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    RectBase() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Draw cached corner textures into a render target
    ///
    /// - \param dst Destination render target
    /// - \param areaCaches Corner textures
    /// - \param cornerPositions Destination corner positions
    ///
    ////////////////////////////////////////////////////////////
    BIND_IGNORE()
    void renderCorners(sf::RenderTarget &dst, const std::vector<sf::Texture *> &areaCaches,
                       const std::vector<sf::Vector2f> &cornerPositions);

    ////////////////////////////////////////////////////////////
    /// \brief Draw edge textures stretched between corners
    ///
    /// - \param dst Destination render target
    /// - \param areaCaches Edge textures
    /// - \param edgePositions Destination edge anchor positions
    ///
    ////////////////////////////////////////////////////////////
    BIND_IGNORE()
    void renderEdges(sf::RenderTarget &dst, const std::vector<sf::Texture *> &areaCaches,
                     const std::vector<sf::Vector2f> &edgePositions);

    ////////////////////////////////////////////////////////////
    /// \brief Compose edge render texture from cached pieces
    ///
    /// - \param edge Render texture used for edge composition
    /// - \param cachedCorners Cached corner textures
    /// - \param cachedEdges Cached edge textures
    ///
    ////////////////////////////////////////////////////////////
    BIND_IGNORE()
    void renderSides(sf::RenderTexture &edge, const std::vector<sf::Texture *> &cachedCorners,
                     const std::vector<sf::Texture *> &cachedEdges);

    ////////////////////////////////////////////////////////////
    /// \brief Render the complete nine-patch rectangle
    ///
    /// - \param dst Destination render texture
    /// - \param edge Intermediate edge texture
    /// - \param edgeSprite Prepared edge sprite
    /// - \param backSprite Prepared background sprite
    /// - \param cachedCorners Cached corner textures
    /// - \param cachedEdges Cached edge textures
    /// - \param renderStates Render states used when drawing
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void render(sf::RenderTexture &dst, sf::RenderTexture &edge, sf::Sprite &edgeSprite, sf::Sprite &backSprite,
                const std::vector<sf::Texture *> &cachedCorners, const std::vector<sf::Texture *> &cachedEdges,
                sf::RenderStates renderStates);
};

////////////////////////////////////////////////////////////
/// \brief Register `RectBase` bindings to a Python module
///
/// - \param m Target Python module
///
////////////////////////////////////////////////////////////
void ApplyRectBaseBinding(py::module &m);
