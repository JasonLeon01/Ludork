#include <GameMapGraphics.h>
#include <SFML/Graphics/Image.hpp>
#include <cstdint>
#include <pybind11/stl.h>

GameMapGraphics::GameMapGraphics(const std::string &shaderPath) {
  materialShader_ = new sf::Shader();
  if (!materialShader_->loadFromFile(shaderPath, sf::Shader::Type::Fragment)) {
    throw std::runtime_error("Failed to load shader from file: " + shaderPath);
  }
}

GameMapGraphics::~GameMapGraphics() { delete materialShader_; }

sf::Shader *GameMapGraphics::getMaterialShader() const {
  return materialShader_;
}

void GameMapGraphics::refreshShader(
    const sf::RenderTexture &lightMask, const sf::Texture &mirrorTex,
    const sf::Texture &reflectionStrengthTex, float screenScale,
    const sf::Vector2f &screenSize, const sf::Vector2f &viewPos, float viewRot,
    const sf::Vector2f &gridSize, int cellSize,
    const std::vector<py::object> &lights, const sf::Color &ambientColor) {
  if (!materialShader_) {
    return;
  }
  auto shader = materialShader_;
  shader->setUniform("lightMask", lightMask.getTexture());
  shader->setUniform("mirrorTex", mirrorTex);
  shader->setUniform("reflectionStrengthTex", reflectionStrengthTex);
  shader->setUniform("screenScale", screenScale);
  shader->setUniform("screenSize", screenSize);
  shader->setUniform("viewPos", viewPos);
  shader->setUniform("viewRot", viewRot);
  shader->setUniform("gridSize", gridSize);
  shader->setUniform("cellSize", cellSize);
  shader->setUniform("lightLen", int(lights.size()));
  for (int i = 0; i < lights.size(); ++i) {
    auto light = lights[i];
    auto c = light.attr("color").cast<sf::Color>();
    shader->setUniform("lightPos[" + std::to_string(i) + "]",
                       light.attr("position").cast<sf::Vector2f>());
    shader->setUniform("lightColor[" + std::to_string(i) + "]",
                       castFromColor(c));
    shader->setUniform("lightRadius[" + std::to_string(i) + "]",
                       light.attr("radius").cast<float>());
    shader->setUniform("lightIntensity[" + std::to_string(i) + "]",
                       light.attr("intensity").cast<float>());
  }
  shader->setUniform("ambientColor", castFromColor(ambientColor));
}

sf::Texture *GameMapGraphics::generateDataFromMap(
    const sf::Vector2u &size,
    const std::vector<std::vector<float>> &materialMap, bool smooth) {
  int dataLen = size.x * size.y * 4;
  std::vector<std::uint8_t> pixelData(dataLen);
  for (int y = 0; y < size.y; ++y) {
    for (int x = 0; x < size.x; ++x) {
      int index = (y * size.x + x) * 4;
      pixelData[index] = std::uint8_t(materialMap[y][x] * 255.0f);
      pixelData[index + 1] = pixelData[index];
      pixelData[index + 2] = pixelData[index];
      pixelData[index + 3] = 255;
    }
  }

  sf::Image img(size, pixelData.data());
  sf::Texture *texture = new sf::Texture();
  if (!texture->loadFromImage(img)) {
    throw std::runtime_error(
        "Failed to load texture from image at method generateDataFromMap");
  }
  texture->setSmooth(smooth);
  return texture;
}

sf::Vector3f GameMapGraphics::castFromColor(const sf::Color &color) {
  return sf::Vector3f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
}

void ApplyGameMapGraphicsBinding(py::module &m) {
  py::class_<GameMapGraphics> GameMapGraphicsClass(m, "GameMapGraphics");
  GameMapGraphicsClass.def(py::init<const std::string &>());
  GameMapGraphicsClass.def("getMaterialShader",
                           &GameMapGraphics::getMaterialShader,
                           py::return_value_policy::reference);
  GameMapGraphicsClass.def(
      "refreshShader", &GameMapGraphics::refreshShader, py::arg("lightMask"),
      py::arg("mirrorTex"), py::arg("reflectionStrengthTex"),
      py::arg("screenScale"), py::arg("screenSize"), py::arg("viewPos"),
      py::arg("viewRot"), py::arg("gridSize"), py::arg("cellSize"),
      py::arg("lights"), py::arg("ambientColor"));
  GameMapGraphicsClass.def(
      "generateDataFromMap", &GameMapGraphics::generateDataFromMap,
      py::arg("size"), py::arg("materialMap"), py::arg("smooth") = false);
}
