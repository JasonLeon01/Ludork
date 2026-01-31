#include <SFML/System/Vector2.hpp>
#include <Tilemap.h>

void C_CalculateVertexArray(sf::VertexArray &vertexArray,
                            const TileGrids &tiles, int tileSize, int columns,
                            int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto tileNumberObj = tiles[y][x];
      if (!tileNumberObj.has_value()) {
        continue;
      }
      int tileNumber = tileNumberObj.value();
      int tu = tileNumber % columns;
      int tv = tileNumber / columns;
      int start = (x + y * width) * 6;
      vertexArray[start + 0].position =
          sf::Vector2f(x * tileSize, y * tileSize);
      vertexArray[start + 1].position =
          sf::Vector2f((x + 1) * tileSize, y * tileSize);
      vertexArray[start + 2].position =
          sf::Vector2f(x * tileSize, (y + 1) * tileSize);
      vertexArray[start + 3].position =
          sf::Vector2f(x * tileSize, (y + 1) * tileSize);
      vertexArray[start + 4].position =
          sf::Vector2f((x + 1) * tileSize, y * tileSize);
      vertexArray[start + 5].position =
          sf::Vector2f((x + 1) * tileSize, (y + 1) * tileSize);
      vertexArray[start + 0].texCoords =
          sf::Vector2f(tu * tileSize, tv * tileSize);
      vertexArray[start + 1].texCoords =
          sf::Vector2f((tu + 1) * tileSize, tv * tileSize);
      vertexArray[start + 2].texCoords =
          sf::Vector2f(tu * tileSize, (tv + 1) * tileSize);
      vertexArray[start + 3].texCoords =
          sf::Vector2f(tu * tileSize, (tv + 1) * tileSize);
      vertexArray[start + 4].texCoords =
          sf::Vector2f((tu + 1) * tileSize, tv * tileSize);
      vertexArray[start + 5].texCoords =
          sf::Vector2f((tu + 1) * tileSize, (tv + 1) * tileSize);
    }
  }
}
