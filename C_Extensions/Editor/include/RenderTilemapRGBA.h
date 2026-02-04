#pragma once

#include <pybind11/pybind11.h>

namespace py = pybind11;

py::bytes C_RenderTilemapRGBA(py::buffer tilesetRgba, int tilesetW,
                              int tilesetH, int tilesetStride, py::buffer tiles,
                              int mapW, int mapH, int tileSize);
