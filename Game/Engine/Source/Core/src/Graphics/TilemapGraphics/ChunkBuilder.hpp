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
void validateMaterialOpacities(const std::vector<Material>& materials,
                               std::string_view source);
sf::Color materialColour(float opacity);
std::size_t chunkIndex(int x, int y, int columns);
int tileVertexOffset(const TileChunk& chunk, int x, int y);
std::array<sf::Vector2f, 6> rectangleVertices(float left, float top,
                                              float right, float bottom);

template <typename ReadMaterial, typename ReadValue>
std::vector<std::vector<float>> materialMap(int width, int height,
                                            ReadMaterial readMaterial,
                                            ReadValue readValue) {
    std::vector<std::vector<float>> result(height,
                                           std::vector<float>(width, 0.0f));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto material = readMaterial(sf::Vector2i(x, y));
            if (material.has_value()) {
                result[y][x] = readValue(*material);
            }
        }
    }
    return result;
}

}  // namespace ludork::engine::tilemap_graphics_impl
