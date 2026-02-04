#define PYBIND11_INTERNALS_ID "PYSF"

#include <RenderTilemapRGBA.h>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(EditorExtension, m) {
  void *internals_ptr = static_cast<void *>(
      const_cast<py::detail::internals *>(&py::detail::get_internals()));
  std::cout << "[Ludork Editor Binding] PyBind11 internals address: "
            << internals_ptr << std::endl;

  m.def("C_RenderTilemapRGBA", &C_RenderTilemapRGBA, py::arg("tilesetRgba"),
        py::arg("tilesetW"), py::arg("tilesetH"), py::arg("tilesetStride"),
        py::arg("tiles"), py::arg("mapW"), py::arg("mapH"),
        py::arg("tileSize"));
}