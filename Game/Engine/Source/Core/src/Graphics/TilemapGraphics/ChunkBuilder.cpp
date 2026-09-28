#include "ChunkBuilder.hpp"

#include "Pattern.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ludork::engine::tilemap_graphics_impl {

void validateMaterialOpacities(const std::vector<Material>& materials,
                               std::string_view source) {
    for (std::size_t index = 0; index < materials.size(); ++index) {
        const float opacity = materials[index].opacity;
        if (!std::isfinite(opacity) || opacity < 0.0f || opacity > 1.0f) {
            throw std::invalid_argument(
                std::string(source) + "[" + std::to_string(index) +
                "].opacity must be finite and in [0, 1]");
        }
    }
}

sf::Color materialColour(float opacity) {
    sf::Color colour = sf::Color::White;
    colour.a = static_cast<std::uint8_t>(opacity * 255.0f);
    return colour;
}

std::size_t chunkIndex(int x, int y, int columns) {
    return static_cast<std::size_t>(y * columns + x);
}

int tileVertexOffset(const TileChunk& chunk, int x, int y) {
    return ((x - chunk.x) + (y - chunk.y) * chunk.width) * 6;
}

std::array<sf::Vector2f, 6> rectangleVertices(float left, float top,
                                              float right, float bottom) {
    return {sf::Vector2f(left, top),    sf::Vector2f(right, top),
            sf::Vector2f(left, bottom), sf::Vector2f(left, bottom),
            sf::Vector2f(right, top),   sf::Vector2f(right, bottom)};
}

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
