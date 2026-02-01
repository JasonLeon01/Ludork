#include <Particles/RemoveParticle.h>

bool C_RemoveParticle(std::vector<py::object> &particles,
                      sf::VertexArray &vertexArray, std::size_t index) {
  int n_before = particles.size();
  bool result = (n_before == 1);
  if (index != n_before - 1) {
    for (int i = index; i < n_before - 1; ++i) {
      int src = (i + 1) * 6;
      int dst = i * 6;
      for (int k = 0; k < 6; ++k) {
        vertexArray[dst + k] = vertexArray[src + k];
      }
    }
    particles.erase(particles.begin() + index);
    vertexArray.resize((n_before - 1) * 6);
  }
  return result;
}