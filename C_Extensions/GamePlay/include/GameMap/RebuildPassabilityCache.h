#pragma once

#include <SFML/System/Vector2.hpp>
#include <map>
#include <pybind11/pybind11.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace py = pybind11;
using TileGrids = std::vector<std::vector<std::optional<int>>>;
using ActorDict = std::unordered_map<std::string, std::vector<py::object>>;

std::pair<std::vector<std::vector<bool>>,
          std::map<std::pair<int, int>, std::vector<py::object>>>
C_RebuildPassabilityCache(
    const sf::Vector2u &size, std::vector<std::string> &layerKeysList,
    const std::unordered_map<std::string, TileGrids> &tileData,
    const ActorDict &actors, py::object &tilemap, py::function &getLayer,
    py::function &isPassable, py::function &getCollisionEnabled,
    py::function &getMapPosition);
