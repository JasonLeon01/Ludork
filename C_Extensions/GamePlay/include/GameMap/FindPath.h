#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>
#include <unordered_map>
#include <vector>

namespace py = pybind11;
using ActorDict = std::unordered_map<std::string, std::vector<py::object>>;
using TileGrids = std::vector<std::vector<std::optional<int>>>;
using IntPair = std::pair<int, int>;

std::vector<sf::Vector2i>
C_FindPath(const sf::Vector2i &start, const sf::Vector2i &goal,
           const sf::Vector2u &size, const py::object &tilemap,
           const std::vector<std::string> &layerKeys, const ActorDict &actors,
           const std::unordered_map<std::string, TileGrids> &tileData,
           const py::function &getLayer, const py::function &getMapPosition,
           const py::function &getCollisionEnabled,
           const py::function &isPassable);