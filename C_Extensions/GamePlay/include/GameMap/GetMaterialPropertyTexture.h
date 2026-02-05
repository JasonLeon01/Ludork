#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>
#include <vector>

namespace py = pybind11;

void C_GetMaterialPropertyTexture(
    const sf::Vector2u &size, sf::Image &img,
    const std::vector<std::vector<py::object>> &materialMap);

void ApplyGetMaterialPropertyTextureBinding(py::module &m);
