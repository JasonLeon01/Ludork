#pragma once

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>
#include <vector>

namespace py = pybind11;

class RectBase {
public:
  void renderCorners(sf::RenderTarget &dst,
                     const std::vector<sf::Texture *> &areaCaches,
                     const std::vector<sf::Vector2f> &cornerPositions);
  void renderEdges(sf::RenderTarget &dst,
                   const std::vector<sf::Texture *> &areaCaches,
                   const std::vector<sf::Vector2f> &edgePositions);
  void renderSides(sf::RenderTexture &edge,
                   const std::vector<sf::Texture *> &cachedCorners,
                   const std::vector<sf::Texture *> &cachedEdges);
  void render(sf::RenderTexture &dst, sf::RenderTexture &edge,
              sf::Sprite &edgeSprite, sf::Sprite &backSprite,
              const std::vector<sf::Texture *> &cachedCorners,
              const std::vector<sf::Texture *> &cachedEdges,
              sf::RenderStates renderStates);
};

void ApplyRectBaseBinding(py::module &m);
