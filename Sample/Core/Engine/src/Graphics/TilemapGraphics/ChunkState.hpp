#pragma once

#include <SFML/Graphics/VertexArray.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace ludork::engine::tilemap_graphics_impl {

struct TileChunk {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::unique_ptr<sf::VertexArray> vertexArray;
    std::vector<std::unique_ptr<sf::VertexArray>> autoTileVertexArrays;
    std::vector<std::vector<std::pair<int, int>>> autoTileCells;
    std::vector<std::vector<int>> autoTileMasks;
};

struct ChunkRange {
    int firstX = 0;
    int firstY = 0;
    int lastX = -1;
    int lastY = -1;
};

}  // namespace ludork::engine::tilemap_graphics_impl
