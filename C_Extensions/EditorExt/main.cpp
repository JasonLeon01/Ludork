#define PYBIND11_INTERNALS_ID "PYSF"

#include <GraphLayout.h>
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
          py::arg("tiles"), py::arg("mapW"), py::arg("mapH"), py::arg("tileSize"));
    m.def("C_ComputeGraphLayoutPositions", &C_ComputeGraphLayoutPositions,
          "Compute organised node positions for a blueprint or common-function graph.",
          py::arg("nodeCount"), py::arg("links"), py::arg("nodeRely"), py::arg("startIdx"),
          py::arg("defaultParamCount") = 0, py::arg("xStep") = 720.0, py::arg("yStep") = 250.0,
          py::arg("defaultParamYStep") = 64.0, py::arg("defaultParamStartY") = 64.0,
          py::arg("startGap") = 250.0);
}
