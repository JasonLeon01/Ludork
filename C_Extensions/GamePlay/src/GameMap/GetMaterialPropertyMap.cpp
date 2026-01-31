#include <GameMap/GetMaterialPropertyMap.h>
#include <SFML/System/Vector2.hpp>

py::object getMaterialProperty(
    const std::vector<std::string> &layerKeys, const py::object &tilemap,
    const ActorDict &actors, const sf::Vector2i pos,
    const std::unordered_map<std::string, TileGrids> &tileData,
    const std::string &functionName, const py::object &invalidValue,
    const py::function &getLayer, const py::function &getMapPosition) {
  for (auto &layerName : layerKeys) {
    auto layer = getLayer(tilemap, layerName);
    if (layer.is_none()) {
      continue;
    }
    if (!layer.attr("visible").cast<bool>()) {
      continue;
    }
    auto it = actors.find(layerName);
    if (it != actors.end()) {
      auto layerActors = it->second;
      for (auto &actor : layerActors) {
        if (getMapPosition(actor).cast<sf::Vector2i>() == pos) {
          auto value = actor.attr(functionName.c_str())();
          if (!value.equal(invalidValue)) {
            return value;
          }
        }
      }
    }
    auto tileObj = tileData.at(layerName)[pos.y][pos.x];
    if (tileObj.has_value()) {
      auto value = layer.attr(functionName.c_str())(pos);
      return value;
    }
  }
  return invalidValue;
}

std::vector<std::vector<py::object>> C_GetMaterialPropertyMap(
    const std::vector<std::string> &layerKeys, int width, int height,
    const py::object &tilemap, const ActorDict &actors,
    const std::unordered_map<std::string, TileGrids> &tileData,
    const std::string &functionName, const py::object &invalidValue,
    const py::function &getLayer, const py::function &getMapPosition) {
  std::vector<std::vector<py::object>> materialPropertyMap;
  for (int y = 0; y < height; ++y) {
    materialPropertyMap.push_back({});
    for (int x = 0; x < width; ++x) {
      materialPropertyMap[y].push_back(getMaterialProperty(
          layerKeys, tilemap, actors, {x, height - y - 1}, tileData,
          functionName, invalidValue, getLayer, getMapPosition));
    }
  }
  return materialPropertyMap;
}
