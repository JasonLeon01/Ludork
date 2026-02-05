#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>

namespace py = pybind11;

void C_AddParticle(const sf::Vector2f &infoPosition,
                   const sf::Angle &infoRotation, const sf::Vector2f &infoScale,
                   const sf::Color &infoColor, const sf::Vector2f &uv_tl,
                   const sf::Vector2f &uv_tr, const sf::Vector2f &uv_br,
                   const sf::Vector2f &uv_bl, const sf::Vector2f &tl_tr,
                   const sf::Vector2f &tr_tr, const sf::Vector2f &br_tr,
                   const sf::Vector2f &bl_tr, sf::VertexArray &vertexArray);

void ApplyAddParticleBinding(py::module &m);
