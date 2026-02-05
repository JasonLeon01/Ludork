#include <Particle.h>

void ApplyParticleBinding(py::module &m) {
  ApplyAddParticleBinding(m);
  ApplyRemoveParticleBinding(m);
  ApplyUpdateParticlesInfoBinding(m);
}
