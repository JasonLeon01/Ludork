#pragma once

#include <pybind11/pybind11.h>

#include <string>
#include <vector>

namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Rasterize a map layer (tile indices + autotiles) into packed RGBA bytes
///
/// `tilesetRgbaTuple` is either ``None`` or ``(rgba, w, h, stride)``.
/// `tiles` is either ``None`` or a 1D ``int32`` tile index buffer.
/// `autoTileGrid` is either ``None`` or a 2D grid of autotile keys.
/// `autoTileSourcesByKey` is either ``None`` or a dict mapping keys to
/// ``(rgba, w, h, stride)`` tuples.
///
/// - \param mapW Map width in tiles
/// - \param mapH Map height in tiles
/// - \param sourceTileSize Tile size in source assets (typically 32)
/// - \param outputTileSize Display tile size in pixels
/// - \param tilesetRgbaTuple Optional tileset RGBA tuple
/// - \param tiles Optional tile index buffer
/// - \param autoTileFrame Autotile animation frame index
/// - \param autoTileGrid Optional autotile key grid
/// - \param autoTileSourcesByKey Optional autotile source buffers
///
/// - \return Packed RGBA8888 bytes for the rendered layer
///
////////////////////////////////////////////////////////////
py::bytes C_RenderMapLayerRGBA(int mapW, int mapH, int sourceTileSize, int outputTileSize,
                               py::object tilesetRgbaTuple, py::object tiles, int autoTileFrame,
                               py::object autoTileGrid, py::object autoTileSourcesByKey);
