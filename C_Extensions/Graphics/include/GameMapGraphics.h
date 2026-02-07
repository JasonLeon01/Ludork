#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <pybind11/pybind11.h>
#include <string>
#include <vector>

namespace py = pybind11;

class GameMapGraphics {
public:
  GameMapGraphics(const std::string &shaderPath);
  ~GameMapGraphics();
  sf::Shader *getMaterialShader() const;
  void refreshShader(const std::vector<sf::RenderTexture *> &canvases,
                     const std::vector<sf::Texture *> &lightBlockTexs,
                     const sf::Texture &mirrorTex,
                     const sf::Texture &reflectionStrengthTex,
                     const sf::Texture &emissiveTex, float screenScale,
                     const sf::Vector2f &screenSize,
                     const sf::Vector2f &viewPos, float viewRot,
                     const sf::Vector2f &gridSize,
                     int cellSize, const std::vector<py::object> &lights,
                     const sf::Color &ambientColor);
  sf::Texture *
  generateDataFromMap(const sf::Vector2u &size,
                      const std::vector<std::vector<float>> &materialMap,
                      bool smooth = false);

private:
  sf::Vector3f castFromColor(const sf::Color &color);
  sf::Shader *materialShader_;
};

void ApplyGameMapGraphicsBinding(py::module &m);
