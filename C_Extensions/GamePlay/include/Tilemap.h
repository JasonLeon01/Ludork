#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <optional>
#include <pybind11/pybind11.h>
#include <vector>


namespace py = pybind11;
using TileGrids = std::vector<std::vector<std::optional<int>>>;

void C_CalculateVertexArray(sf::VertexArray &vertexArray,
                            const TileGrids &tiles,
                            const std::vector<py::object> &materials,
                            int tileSize, int columns, int width, int height);