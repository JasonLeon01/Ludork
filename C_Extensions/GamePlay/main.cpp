#define PYBIND11_INTERNALS_ID "PYSF"

#include <GameMap.h>
#include <Particle.h>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(GamePlayExtension, m) {
  void *internals_ptr = static_cast<void *>(
      const_cast<py::detail::internals *>(&py::detail::get_internals()));
  std::cout << "[Ludork PYSF Binding] PyBind11 internals address: "
            << internals_ptr << std::endl;

  auto importModule = py::module::import("importlib").attr("import_module");
  try {
    importModule("Engine.pysf");
  } catch (py::error_already_set &) {
    PyErr_Clear();
    importModule("pysf");
  }

  ApplyGameMapBinding(m);
  ApplyParticleBinding(m);
}
