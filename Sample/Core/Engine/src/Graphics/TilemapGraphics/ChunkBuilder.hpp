#pragma once

#include "ChunkState.hpp"

#include <General/TileLayerData.hpp>

#include <optional>
#include <vector>

namespace ludork::engine::tilemap_graphics_impl {

std::vector<TileChunk> createChunks(int width, int height, int chunkSize);
std::optional<int> autoTileIndexAt(const AutoTileGrid& grid, int x, int y);
int autoTileMask(const AutoTileGrid& grid, int x, int y, int poolIndex);

}  // namespace ludork::engine::tilemap_graphics_impl
