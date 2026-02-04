#define PYBIND11_INTERNALS_ID "PYSF"

#include <CompressAnimation.h>
#include <RectBase.h>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(GraphicsExtension, m) {
  void *internals_ptr = static_cast<void *>(
      const_cast<py::detail::internals *>(&py::detail::get_internals()));
  std::cout << "[Ludork PYSF Binding] PyBind11 internals address: "
            << internals_ptr << std::endl;

  m.def("C_CompressAnimation", &C_CompressAnimation, py::arg("zlibModule"),
        py::arg("frameCount"), py::arg("frameStep"), py::arg("frameRate"),
        py::arg("timeLines"), py::arg("assets"), py::arg("assetsRoot"),
        py::arg("imageFormat"));

  py::class_<RectBase> RectBaseClass(m, "RectBase");
  RectBaseClass.def(py::init<>());
  RectBaseClass.def("renderCorners", &RectBase::renderCorners, py::arg("dst"),
                    py::arg("areaCaches"), py::arg("cornerPositions"));
  RectBaseClass.def("renderEdges", &RectBase::renderEdges, py::arg("dst"),
                    py::arg("areaCaches"), py::arg("edgePositions"));
  RectBaseClass.def("renderSides", &RectBase::renderSides, py::arg("dst"),
                    py::arg("cachedCorners"), py::arg("cachedEdges"));
  RectBaseClass.def("render", &RectBase::render, py::arg("dst"),
                    py::arg("edge"), py::arg("edgeSprite"),
                    py::arg("backSprite"), py::arg("cachedCorners"),
                    py::arg("cachedEdges"), py::arg("renderStates"));
}