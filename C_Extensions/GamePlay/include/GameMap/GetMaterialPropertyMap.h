#pragma once

#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>
#include <unordered_map>
#include <vector>

namespace py = pybind11;
using ActorDict = std::unordered_map<std::string, std::vector<py::object>>;
using TileGrids = std::vector<std::vector<std::optional<int>>>;

std::vector<std::vector<py::object>> C_GetMaterialPropertyMap(
    const std::vector<std::string> &layerKeys, int width, int height,
    const py::object &tilemap, const ActorDict &actors,
    const std::unordered_map<std::string, TileGrids> &tileData,
    const std::string &functionName, const py::object &invalidValue,
    const py::function &getLayer, const py::function &getMapPosition);

void ApplyGetMaterialPropertyMapBinding(py::module &m);
