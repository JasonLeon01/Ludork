#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <pybind11/pybind11.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace py = pybind11;
using Particle = std::unordered_map<std::string, std::vector<py::object>>;

void C_UpdateParticlesInfo(
    py::function getUpdateParticleInfo,
    const std::vector<py::object> &updateFlags, const Particle &particles,
    const std::unordered_map<std::string, sf::VertexArray *> &vertexArrays);
