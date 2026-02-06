#include <SFML/Graphics/PrimitiveType.hpp>
#include <TilemapGraphics.h>
#include <pybind11/stl.h>

TileLayerGraphics::TileLayerGraphics(
    int width, int height, int tileSize, sf::Texture *texture,
    const std::vector<std::vector<std::optional<int>>> &tiles,
    const std::vector<py::object> &materials) {
  texture_ = texture;
  vertexArray_ =
      new sf::VertexArray(sf::PrimitiveType::Triangles, width * height * 6);
  size_ = sf::Vector2f(width, height);
  tiles_ = tiles;
  materials_ = materials;
  init(tileSize);
}

TileLayerGraphics::~TileLayerGraphics() {
  delete texture_;
  delete vertexArray_;
}

void TileLayerGraphics::draw(sf::RenderTarget &target,
                             sf::RenderStates states) const {
  states.transform *= getTransform();
  states.texture = texture_;
  target.draw(*vertexArray_, states);
}

void TileLayerGraphics::init(int tileSize) {
  int columns = texture_->getSize().x / tileSize;
  int width = size_.x;
  int height = size_.y;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto tileNumberObj = tiles_[y][x];
      if (!tileNumberObj.has_value()) {
        continue;
      }
      int tileNumber = tileNumberObj.value();

      int tu = tileNumber % columns;
      int tv = tileNumber / columns;
      int start = (x + y * width) * 6;

      float opacity = materials_[tileNumber].attr("opacity").cast<float>();
      sf::Color color = sf::Color::White;
      color.a = static_cast<std::uint8_t>(opacity * 255);

      std::vector<sf::Vector2f> positions = {
          sf::Vector2f(x * tileSize, y * tileSize),
          sf::Vector2f((x + 1) * tileSize, y * tileSize),
          sf::Vector2f(x * tileSize, (y + 1) * tileSize),
          sf::Vector2f(x * tileSize, (y + 1) * tileSize),
          sf::Vector2f((x + 1) * tileSize, y * tileSize),
          sf::Vector2f((x + 1) * tileSize, (y + 1) * tileSize),
      };
      std::vector<sf::Vector2f> texCoords = {
          sf::Vector2f(tu * tileSize, tv * tileSize),
          sf::Vector2f((tu + 1) * tileSize, tv * tileSize),
          sf::Vector2f(tu * tileSize, (tv + 1) * tileSize),
          sf::Vector2f(tu * tileSize, (tv + 1) * tileSize),
          sf::Vector2f((tu + 1) * tileSize, tv * tileSize),
          sf::Vector2f((tu + 1) * tileSize, (tv + 1) * tileSize),
      };

      for (int i = 0; i < 6; ++i) {
        if (opacity > 0.0f) {
          (*vertexArray_)[start + i].position = positions[i];
          (*vertexArray_)[start + i].texCoords = texCoords[i];
          if (opacity < 1.0f) {
            (*vertexArray_)[start + i].color = color;
          }
        }
      }
    }
  }
}

void ApplyTileLayerGraphicsBinding(py::module &m) {
  py::class_<TileLayerGraphics, sf::Drawable, sf::Transformable>
      TileLayerGraphicsClass(m, "TileLayerGraphics");
  TileLayerGraphicsClass.def(
      py::init<int, int, int, sf::Texture *,
               std::vector<std::vector<std::optional<int>>>,
               std::vector<py::object>>(),
      py::arg("width"), py::arg("height"), py::arg("tileSize"),
      py::arg("texture"), py::arg("tiles"), py::arg("materials"));
}
