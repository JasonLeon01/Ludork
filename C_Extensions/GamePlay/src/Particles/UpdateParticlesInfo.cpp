#include <Particles/UpdateParticlesInfo.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>

void C_UpdateParticlesInfo(
    py::function getUpdateParticleInfo,
    const std::vector<py::object> &updateFlags, const Particle &particles,
    const std::unordered_map<std::string, sf::VertexArray *> &vertexArrays) {
  for (auto &particle : updateFlags) {
    std::string resourcePath =
        particle.attr("resourcePath").cast<std::string>();
    const auto &particleList = particles.at(resourcePath);
    int idx = std::find(particleList.begin(), particleList.end(), particle) -
              particleList.begin();
    if (idx >= static_cast<int>(particleList.size())) {
      continue;
    }
    sf::Transform t = sf::Transform();
    t.translate(particle.attr("info").attr("position").cast<sf::Vector2f>());
    t.rotate(particle.attr("info").attr("rotation").cast<sf::Angle>());
    t.scale(particle.attr("info").attr("scale").cast<sf::Vector2f>());
    auto infoColor = particle.attr("info").attr("color").cast<sf::Color>();
    py::tuple infos = getUpdateParticleInfo(particle);
    sf::Vector2f tl_tr = infos[0].cast<sf::Vector2f>();
    sf::Vector2f tr_tr = infos[1].cast<sf::Vector2f>();
    sf::Vector2f br_tr = infos[2].cast<sf::Vector2f>();
    sf::Vector2f bl_tr = infos[3].cast<sf::Vector2f>();
    sf::Vector2f tl = t.transformPoint(tl_tr);
    sf::Vector2f tr = t.transformPoint(tr_tr);
    sf::Vector2f br = t.transformPoint(br_tr);
    sf::Vector2f bl = t.transformPoint(bl_tr);

    auto it = vertexArrays.find(resourcePath);
    if (it == vertexArrays.end() || it->second == nullptr) {
      continue;
    }
    auto vertexArray = it->second;
    (*vertexArray)[idx * 6 + 0].position = tl;
    (*vertexArray)[idx * 6 + 1].position = tr;
    (*vertexArray)[idx * 6 + 2].position = br;
    (*vertexArray)[idx * 6 + 3].position = tl;
    (*vertexArray)[idx * 6 + 4].position = br;
    (*vertexArray)[idx * 6 + 5].position = bl;
    (*vertexArray)[idx * 6 + 0].color = infoColor;
    (*vertexArray)[idx * 6 + 1].color = infoColor;
    (*vertexArray)[idx * 6 + 2].color = infoColor;
    (*vertexArray)[idx * 6 + 3].color = infoColor;
    (*vertexArray)[idx * 6 + 4].color = infoColor;
    (*vertexArray)[idx * 6 + 5].color = infoColor;
  }
}
