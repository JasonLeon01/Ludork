#pragma once

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <optional>
#include <pybind11/pybind11.h>
#include <string>
#include <vector>

namespace py = pybind11;

class TileLayerGraphics : public sf::Drawable, public sf::Transformable {
public:
  TileLayerGraphics(int width, int height, int tileSize, sf::Texture *texture,
                    const std::vector<std::vector<std::optional<int>>> &tiles,
                    const std::vector<py::object> &materials);
  ~TileLayerGraphics();

private:
  void init(int tileSize);
  virtual void draw(sf::RenderTarget &target,
                    sf::RenderStates states) const override;

  sf::VertexArray *vertexArray_;
  sf::Texture *texture_;
  sf::Vector2f size_;
  std::vector<std::vector<std::optional<int>>> tiles_;
  std::vector<py::object> materials_;
};

void ApplyTileLayerGraphicsBinding(py::module &m);
