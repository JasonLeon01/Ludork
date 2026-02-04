#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <Tilemap.h>

void C_CalculateVertexArray(sf::VertexArray &vertexArray,
                            const TileGrids &tiles,
                            const std::vector<py::object> &materials,
                            int tileSize, int columns, int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto tileNumberObj = tiles[y][x];
      if (!tileNumberObj.has_value()) {
        continue;
      }
      int tileNumber = tileNumberObj.value();
      int tu = tileNumber % columns;
      int tv = tileNumber / columns;
      float opacity = materials[tileNumber].attr("opacity").cast<float>();
      sf::Color color = sf::Color(255, 255, 255, int(opacity * 255));
      int start = (x + y * width) * 6;
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
        vertexArray[start + i].position = positions[i];
        vertexArray[start + i].texCoords = texCoords[i];
        if (opacity < 255) {
          vertexArray[start + i].color = color;
        }
      }
    }
  }
}
