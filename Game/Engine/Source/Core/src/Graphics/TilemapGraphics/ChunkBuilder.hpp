#pragma once

#include "ChunkState.hpp"

#include <General/TileLayerData.hpp>

#include <optional>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ludork::engine::tilemap_graphics_impl {

std::vector<TileChunk> createChunks(int width, int height, int chunkSize);
std::optional<int> autoTileIndexAt(const AutoTileGrid& grid, int x, int y);
int autoTileMask(const AutoTileGrid& grid, int x, int y, int poolIndex);
std::size_t chunkIndex(int x, int y, int columns);
int tileVertexOffset(const TileChunk& chunk, int x, int y);
std::array<sf::Vector2f, 6> rectangleVertices(float left, float top,
                                              float right, float bottom);

}  // namespace ludork::engine::tilemap_graphics_impl
