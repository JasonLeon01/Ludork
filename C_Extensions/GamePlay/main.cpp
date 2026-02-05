#define PYBIND11_INTERNALS_ID "PYSF"

#include <GameMap.h>
#include <Particle.h>
#include <Tilemap.h>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(GamePlayExtension, m) {
  void *internals_ptr = static_cast<void *>(
      const_cast<py::detail::internals *>(&py::detail::get_internals()));
  std::cout << "[Ludork PYSF Binding] PyBind11 internals address: "
            << internals_ptr << std::endl;
  ApplyCalculateVertexArrayBinding(m);
  ApplyGameMapBinding(m);
  ApplyParticleBinding(m);
}