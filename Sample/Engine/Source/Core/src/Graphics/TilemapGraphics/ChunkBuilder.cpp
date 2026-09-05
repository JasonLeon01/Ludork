#include "ChunkBuilder.hpp"

#include "Pattern.hpp"

#include <algorithm>

namespace ludork::engine::tilemap_graphics_impl {

std::vector<TileChunk> createChunks(int width, int height, int chunkSize) {
    const int columns = width > 0 ? (width + chunkSize - 1) / chunkSize : 0;
    const int rows = height > 0 ? (height + chunkSize - 1) / chunkSize : 0;
    std::vector<TileChunk> chunks;
    chunks.reserve(static_cast<std::size_t>(columns * rows));
    for (int chunkY = 0; chunkY < rows; ++chunkY) {
        for (int chunkX = 0; chunkX < columns; ++chunkX) {
            TileChunk chunk;
            chunk.x = chunkX * chunkSize;
            chunk.y = chunkY * chunkSize;
            chunk.width = std::min(chunkSize, width - chunk.x);
            chunk.height = std::min(chunkSize, height - chunk.y);
            chunks.push_back(std::move(chunk));
        }
    }
    return chunks;
}

std::optional<int> autoTileIndexAt(const AutoTileGrid& grid, int x, int y) {
    if (y < 0 || y >= static_cast<int>(grid.size())) {
        return std::nullopt;
    }
    const auto& row = grid[y];
    if (x < 0 || x >= static_cast<int>(row.size())) {
        return std::nullopt;
    }
    const auto& cell = row[x];
    if (!cell.has_value()) {
        return std::nullopt;
    }
    if (const auto index = std::get_if<int>(&cell.value())) {
        return *index;
    }
    return std::nullopt;
}

int autoTileMask(const AutoTileGrid& grid, int x, int y, int poolIndex) {
    const auto sameAt = [&grid, poolIndex](int cellX, int cellY) {
        const std::optional<int> other = autoTileIndexAt(grid, cellX, cellY);
        return other.has_value() && *other == poolIndex;
    };
    int mask = 0;
    if (sameAt(x, y - 1)) {
        mask |= kMaskTop;
    }
    if (sameAt(x + 1, y)) {
        mask |= kMaskRight;
    }
    if (sameAt(x, y + 1)) {
        mask |= kMaskBottom;
    }
    if (sameAt(x - 1, y)) {
        mask |= kMaskLeft;
    }
    if (sameAt(x - 1, y - 1)) {
        mask |= kMaskTopLeft;
    }
    if (sameAt(x + 1, y - 1)) {
        mask |= kMaskTopRight;
    }
    if (sameAt(x + 1, y + 1)) {
        mask |= kMaskBottomRight;
    }
    if (sameAt(x - 1, y + 1)) {
        mask |= kMaskBottomLeft;
    }
    return mask;
}

}  // namespace ludork::engine::tilemap_graphics_impl
