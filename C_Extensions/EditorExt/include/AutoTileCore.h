#pragma once

#include <MapImageBuffer.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

void renderAutoTileLayerOntoBuffer(
    int mapW, int mapH, int sourceTileSize, int frame,
    const std::vector<std::vector<std::string>> &autoTileGrid,
    const std::unordered_map<std::string, RgbaImageView> &sources, std::uint8_t *dst, int dstW,
    int dstH, int dstStride);

int computeAutoTileMaskFromGrid(const std::vector<std::vector<std::string>> &grid, int x, int y);

int normalizeAutoTileMaskValue(int mask);

void composeAutoTileTileIntoBuffer(const RgbaImageView &src, int mask, int frame, int tileSize,
                                   std::uint8_t *out, int outStride);
