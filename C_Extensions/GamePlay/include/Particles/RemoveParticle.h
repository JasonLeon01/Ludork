#pragma once

#include <SFML/Graphics/VertexArray.hpp>
#include <pybind11/pybind11.h>
#include <vector>

namespace py = pybind11;

bool C_RemoveParticle(std::vector<py::object> &particles,
                      sf::VertexArray &vertexArray, std::size_t index);