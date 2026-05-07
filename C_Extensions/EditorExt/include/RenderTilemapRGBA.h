#pragma once

#include <pybind11/pybind11.h>

namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Rasterize a tilemap into an RGBA image buffer
///
/// Both `tilesetRgba` and `tiles` are expected to be 1D buffers.
///
/// - \param tilesetRgba Raw tileset RGBA bytes
/// - \param tilesetW Tileset width in pixels
/// - \param tilesetH Tileset height in pixels
/// - \param tilesetStride Byte stride per tileset row
/// - \param tiles Tile index buffer (`int32`)
/// - \param mapW Tilemap width in tiles
/// - \param mapH Tilemap height in tiles
/// - \param tileSize Tile size in pixels
///
/// - \return Packed RGBA bytes for the rendered map
///
////////////////////////////////////////////////////////////
py::bytes C_RenderTilemapRGBA(py::buffer tilesetRgba, int tilesetW,
                              int tilesetH, int tilesetStride, py::buffer tiles,
                              int mapW, int mapH, int tileSize);
