#pragma once

#include <pybind11/pybind11.h>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <vector>


namespace py = pybind11;

// BIND_CLASS
// Utility class for rendering nine-patch style rectangles.
class RectBase {
public:
    // BIND_INIT
    RectBase() = default;

    // BIND_IGNORE
    void renderCorners(sf::RenderTarget &dst, const std::vector<sf::Texture *> &areaCaches,
                       const std::vector<sf::Vector2f> &cornerPositions);

    // BIND_IGNORE
    void renderEdges(sf::RenderTarget &dst, const std::vector<sf::Texture *> &areaCaches,
                     const std::vector<sf::Vector2f> &edgePositions);

    // BIND_IGNORE
    void renderSides(sf::RenderTexture &edge, const std::vector<sf::Texture *> &cachedCorners,
                     const std::vector<sf::Texture *> &cachedEdges);

    // BIND_METHOD
    // Render the complete nine-patch rectangle.
    void render(sf::RenderTexture &dst, sf::RenderTexture &edge, sf::Sprite &edgeSprite, sf::Sprite &backSprite,
                const std::vector<sf::Texture *> &cachedCorners, const std::vector<sf::Texture *> &cachedEdges,
                sf::RenderStates renderStates);
};

void ApplyRectBaseBinding(py::module &m);
