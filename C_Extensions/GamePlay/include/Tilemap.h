#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <optional>
#include <pybind11/pybind11.h>

using TileGrids = std::vector<std::vector<std::optional<int>>>;

void C_CalculateVertexArray(sf::VertexArray &vertexArray,
                            const TileGrids &tiles, int tileSize, int columns,
                            int width, int height);