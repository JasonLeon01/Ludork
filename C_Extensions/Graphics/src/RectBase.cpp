#include <RectBase.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <pybind11/stl.h>

void RectBase::renderCorners(sf::RenderTarget &dst,
                             const std::vector<sf::Texture *> &areaCaches,
                             const std::vector<sf::Vector2f> &cornerPositions) {
  for (int i = 0; i < 4; ++i) {
    sf::Sprite sprite(*areaCaches[i]);
    sprite.setPosition(cornerPositions[i]);
    dst.draw(sprite);
  }
}

void RectBase::renderEdges(sf::RenderTarget &dst,
                           const std::vector<sf::Texture *> &areaCaches,
                           const std::vector<sf::Vector2f> &edgePositions) {
  float cornerW = edgePositions[0].x;
  float cornerH = edgePositions[2].y;
  float w = dst.getSize().x;
  float h = dst.getSize().y;
  for (int i = 0; i < 4; ++i) {
    sf::Sprite edgeSprite(*areaCaches[i]);
    int rectW, rectH;
    if (i < 2) {
      rectW = int(w - 2 * cornerW);
      rectH = int(areaCaches[i]->getSize().y);
    } else {
      rectW = int(areaCaches[i]->getSize().x);
      rectH = int(h - 2 * cornerH);
    }

    if (rectW < 0)
      rectW = 0;
    if (rectH < 0)
      rectH = 0;

    edgeSprite.setTextureRect(
        sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(rectW, rectH)));
    edgeSprite.setPosition(edgePositions[i]);
    dst.draw(edgeSprite);
  }
}

void RectBase::renderSides(sf::RenderTexture &edge,
                           const std::vector<sf::Texture *> &cachedCorners,
                           const std::vector<sf::Texture *> &cachedEdges) {
  edge.clear(sf::Color::Transparent);
  auto canvasSize = edge.getSize();
  std::vector<sf::Vector2f> cornerPositions = {
      sf::Vector2f(0, 0),
      sf::Vector2f(float(canvasSize.x) - float(cachedCorners[1]->getSize().x),
                   0),
      sf::Vector2f(0,
                   float(canvasSize.y) - float(cachedCorners[2]->getSize().y)),
      sf::Vector2f(float(canvasSize.x) - float(cachedCorners[3]->getSize().x),
                   float(canvasSize.y) - float(cachedCorners[3]->getSize().y))};
  std::vector<sf::Vector2f> edgePositions = {
      sf::Vector2f(float(cachedCorners[0]->getSize().x), 0),
      sf::Vector2f(float(cachedCorners[1]->getSize().x),
                   float(canvasSize.y) - float(cachedCorners[1]->getSize().y)),
      sf::Vector2f(0, float(cachedCorners[2]->getSize().y)),
      sf::Vector2f(float(canvasSize.x) - float(cachedCorners[3]->getSize().x),
                   float(cachedCorners[3]->getSize().y))};
  renderCorners(edge, cachedCorners, cornerPositions);
  renderEdges(edge, cachedEdges, edgePositions);
  edge.display();
}

void RectBase::render(sf::RenderTexture &dst, sf::RenderTexture &edge,
                      sf::Sprite &edgeSprite, sf::Sprite &backSprite,
                      const std::vector<sf::Texture *> &cachedCorners,
                      const std::vector<sf::Texture *> &cachedEdges,
                      sf::RenderStates renderStates) {
  renderSides(edge, cachedCorners, cachedEdges);
  dst.clear(sf::Color::Transparent);
  dst.draw(backSprite, renderStates);
  dst.draw(edgeSprite, renderStates);
  dst.display();
}

void ApplyRectBaseBinding(py::module &m) {
  py::class_<RectBase> RectBaseClass(m, "RectBase");
  RectBaseClass.def(py::init<>());
  RectBaseClass.def("render", &RectBase::render, py::arg("dst"),
                    py::arg("edge"), py::arg("edgeSprite"),
                    py::arg("backSprite"), py::arg("cachedCorners"),
                    py::arg("cachedEdges"), py::arg("renderStates"));
}
