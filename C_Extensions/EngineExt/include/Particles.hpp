#pragma once

#include <pybind11/pybind11.h>

#include "Particles/Particle.hpp"
#include "Particles/ParticleBase.hpp"
#include "Particles/ParticleSystem.hpp"
#include "Particles/TextParticle.hpp"


namespace py = pybind11;

void ApplyParticlesBinding(py::module &m);
