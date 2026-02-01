#include <GameMap/RebuildPassabilityCache.h>

std::pair<std::vector<std::vector<bool>>,
          std::map<std::pair<int, int>, std::vector<py::object>>>
C_RebuildPassabilityCache(
    const sf::Vector2u &size, std::vector<std::string> &layerKeysList,
    const std::unordered_map<std::string, TileGrids> &tileData,
    const ActorDict &actors, py::object &tilemap, py::function &getLayer,
    py::function &isPassable, py::function &getCollisionEnabled,
    py::function &getMapPosition) {
  unsigned int width = size.x;
  unsigned int height = size.y;
  std::vector<std::vector<bool>> tilePassableGrid(height);
  std::map<std::pair<int, int>, std::vector<py::object>> occupancyMap;
  for (unsigned int y = 0; y < height; ++y) {
    std::vector<bool> row(width);
    for (unsigned int x = 0; x < width; ++x) {
      bool passable = true;
      for (auto &layerName : layerKeysList) {
        auto tile = tileData.at(layerName).at(y).at(x);
        auto layer = getLayer(tilemap, layerName);
        if (tile.has_value()) {
          passable = isPassable(layer, sf::Vector2i(x, y)).cast<bool>();
          break;
        }
      }
      row[x] = passable;
    }
    tilePassableGrid[y] = row;
  }
  for (auto &[_, actorList] : actors) {
    for (auto &other : actorList) {
      if (!getCollisionEnabled(other).cast<bool>()) {
        continue;
      }
      auto pos = getMapPosition(other).cast<sf::Vector2i>();
      auto key = std::make_pair(pos.x, pos.y);
      if (occupancyMap.find(key) == occupancyMap.end()) {
        occupancyMap[key] = std::vector<py::object>({other});
      }
      occupancyMap[key].push_back(other);
    }
  }
  return {tilePassableGrid, occupancyMap};
}