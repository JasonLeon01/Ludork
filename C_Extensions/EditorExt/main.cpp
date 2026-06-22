#define PYBIND11_INTERNALS_ID "PYSF"

#include <AutoTile.h>
#include <GraphLayout.h>
#include <MapLayer.h>
#include <RenderTilemapRGBA.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>

namespace py = pybind11;

PYBIND11_MODULE(EditorExt, m) {
    void *internals_ptr = static_cast<void *>(const_cast<py::detail::internals *>(&py::detail::get_internals()));
    std::cout << "[Ludork Editor Binding] PyBind11 internals address: " << internals_ptr << std::endl;

    m.def("C_RenderTilemapRGBA", &C_RenderTilemapRGBA,
          "Rasterize a tile index map into packed RGBA bytes.",
          py::arg("tilesetRgba"), py::arg("tilesetW"), py::arg("tilesetH"), py::arg("tilesetStride"),
          py::arg("tiles"), py::arg("mapW"), py::arg("mapH"), py::arg("sourceTileSize"),
          py::arg("outputTileSize"));
    m.def("C_RenderMapLayerRGBA", &C_RenderMapLayerRGBA,
          "Rasterize a map layer (tiles and autotiles) into packed RGBA bytes.",
          py::arg("mapW"), py::arg("mapH"), py::arg("sourceTileSize"), py::arg("outputTileSize"),
          py::arg("tilesetRgbaTuple"), py::arg("tiles"), py::arg("autoTileFrame"),
          py::arg("autoTileGrid"), py::arg("autoTileSourcesByKey"));
    m.def("C_ComputeGraphLayoutPositions", &C_ComputeGraphLayoutPositions,
          "Compute organised node positions for a blueprint or common-function graph.",
          py::arg("nodeCount"), py::arg("links"), py::arg("nodeRely"), py::arg("startIdx"),
          py::arg("defaultParamCount") = 0, py::arg("nodeHeights") = std::vector<double>{},
          py::arg("xStep") = 720.0, py::arg("yStep") = 320.0, py::arg("defaultParamYStep") = 64.0,
          py::arg("defaultParamStartY") = 64.0, py::arg("startGap") = 250.0, py::arg("columnPadding") = 24.0);
    m.def("C_NormalizeAutoTileMask", &C_NormalizeAutoTileMask,
          "Normalise an 8-bit autotile neighbour mask.",
          py::arg("mask"));
    m.def("C_ComposeAutoTileRGBA", &C_ComposeAutoTileRGBA,
          "Compose one autotile tile into packed RGBA bytes.",
          py::arg("sourceRgba"), py::arg("sourceW"), py::arg("sourceH"), py::arg("sourceStride"),
          py::arg("mask"), py::arg("frame"), py::arg("tileSize"));
    m.def("C_ComputeAutoTileMaskFromGrid", &C_ComputeAutoTileMaskFromGrid,
          "Compute the 8-direction connectivity mask for one autotile grid cell.",
          py::arg("grid"), py::arg("x"), py::arg("y"));
    m.def("C_RenderAutoTileLayerRGBA", &C_RenderAutoTileLayerRGBA,
          "Rasterise an entire autotile layer into packed RGBA bytes.",
          py::arg("mapW"), py::arg("mapH"), py::arg("tileSize"), py::arg("frame"),
          py::arg("autoTileGrid"), py::arg("sourceRgbaByKey"));
}
