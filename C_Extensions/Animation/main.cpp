#include <CompressAnimation.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(AnimationExtension, m) {
  m.def("C_CompressAnimation", &C_CompressAnimation, py::arg("zlibModule"),
        py::arg("frameCount"), py::arg("frameStep"), py::arg("frameRate"),
        py::arg("timeLines"), py::arg("assets"), py::arg("assetsRoot"),
        py::arg("imageFormat"));
}