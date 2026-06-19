#pragma once

#include <MapImageBuffer.h>

#include <pybind11/pybind11.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace py = pybind11;

void renderTilemapOntoBuffer(const RgbaImageView &tileset, const std::int32_t *tiles, int mapW,
                             int mapH, int sourceTileSize, std::uint8_t *dst, int dstW, int dstH,
                             int dstStride);

////////////////////////////////////////////////////////////
/// \brief Rasterize a tile index map into packed RGBA bytes
///
/// - \param tilesetRgba Raw tileset RGBA bytes
/// - \param tilesetW Tileset width in pixels
/// - \param tilesetH Tileset height in pixels
/// - \param tilesetStride Byte stride per tileset row
/// - \param tiles Tile index buffer (`int32`)
/// - \param mapW Tilemap width in tiles
/// - \param mapH Tilemap height in tiles
/// - \param sourceTileSize Tile size in the tileset image
/// - \param outputTileSize Tile size on the rendered map
///
/// - \return Packed RGBA8888 bytes for the rendered map
///
////////////////////////////////////////////////////////////
py::bytes C_RenderTilemapRGBA(py::buffer tilesetRgba, int tilesetW, int tilesetH, int tilesetStride,
                              py::buffer tiles, int mapW, int mapH, int sourceTileSize,
                              int outputTileSize);
