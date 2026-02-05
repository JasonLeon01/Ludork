#define PYBIND11_INTERNALS_ID "PYSF"

#include <Color.h>
#include <CompressAnimation.h>
#include <RectBase.h>
#include <iostream>
#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(GraphicsExtension, m) {
  void *internals_ptr = static_cast<void *>(
      const_cast<py::detail::internals *>(&py::detail::get_internals()));
  std::cout << "[Ludork PYSF Binding] PyBind11 internals address: "
            << internals_ptr << std::endl;

  ApplyCompressAnimationBinding(m);
  ApplyRectBaseBinding(m);
  ApplyColorBinding(m);
}