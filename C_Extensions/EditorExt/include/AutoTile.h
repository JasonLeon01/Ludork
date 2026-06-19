#pragma once

#include <pybind11/pybind11.h>

#include <string>
#include <vector>

namespace py = pybind11;

////////////////////////////////////////////////////////////
/// \brief Normalise an 8-bit autotile neighbour mask
///
/// - \param mask Raw 8-bit neighbour mask
///
/// - \return Normalised mask suitable as a cache key
///
////////////////////////////////////////////////////////////
int C_NormalizeAutoTileMask(int mask);

////////////////////////////////////////////////////////////
/// \brief Compose one autotile tile into packed RGBA bytes
///
/// - \param sourceRgba Source autotile sheet RGBA bytes
/// - \param sourceW Source width in pixels
/// - \param sourceH Source height in pixels
/// - \param sourceStride Byte stride per source row
/// - \param mask 8-bit neighbour mask
/// - \param frame Animation frame index
/// - \param tileSize Output tile size in pixels (typically 32)
///
/// - \return Packed RGBA8888 bytes (`tileSize * tileSize * 4`)
///
////////////////////////////////////////////////////////////
py::bytes C_ComposeAutoTileRGBA(py::buffer sourceRgba, int sourceW, int sourceH,
                                int sourceStride, int mask, int frame, int tileSize);

////////////////////////////////////////////////////////////
/// \brief Compute the 8-direction connectivity mask for one grid cell
///
/// - \param grid 2D grid of autotile keys (empty string = no cell)
/// - \param x Grid x coordinate
/// - \param y Grid y coordinate
///
/// - \return 8-bit neighbour mask
///
////////////////////////////////////////////////////////////
int C_ComputeAutoTileMaskFromGrid(const std::vector<std::vector<std::string>> &grid,
                                  int x, int y);

////////////////////////////////////////////////////////////
/// \brief Rasterise an entire autotile layer into packed RGBA bytes
///
/// `sourceRgbaByKey` maps each autotile key to a 4-tuple:
/// `(rgba_bytes, width, height, stride)`.
///
/// - \param mapW Layer width in tiles
/// - \param mapH Layer height in tiles
/// - \param tileSize Tile size in pixels
/// - \param frame Animation frame index
/// - \param autoTileGrid 2D grid of autotile keys
/// - \param sourceRgbaByKey Per-key source image buffers
///
/// - \return Packed RGBA8888 bytes for the full layer
///
////////////////////////////////////////////////////////////
py::bytes C_RenderAutoTileLayerRGBA(int mapW, int mapH, int tileSize, int frame,
                                    const std::vector<std::vector<std::string>> &autoTileGrid,
                                    const py::dict &sourceRgbaByKey);
