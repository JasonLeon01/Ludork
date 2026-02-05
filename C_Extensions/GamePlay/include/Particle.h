#pragma once

#include <Particles/AddParticle.h>
#include <Particles/RemoveParticle.h>
#include <Particles/UpdateParticlesInfo.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

void ApplyParticleBinding(py::module &m);
