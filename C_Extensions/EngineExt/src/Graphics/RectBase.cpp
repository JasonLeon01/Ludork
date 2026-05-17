#include <pybind11/stl.h>

#include <Graphics/RectBase.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>

#include <algorithm>

void RectBase::renderCorners(sf::RenderTarget &dst, const std::vector<sf::Texture *> &areaCaches,
                             const std::vector<sf::Vector2f> &cornerPositions) {
    for (int i = 0; i < 4; ++i) {
        sf::Sprite sprite(*areaCaches[i]);
        sprite.setPosition(cornerPositions[i]);
        dst.draw(sprite);
    }
}

void RectBase::renderEdges(sf::RenderTarget &dst, const std::vector<sf::Texture *> &areaCaches,
                           const std::vector<sf::Vector2f> &edgePositions) {
    float cornerW = edgePositions[0].x;
    float cornerH = edgePositions[2].y;
    float w = dst.getSize().x;
    float h = dst.getSize().y;
    for (int i = 0; i < 4; ++i) {
        int totalW, totalH;
        int tileW = int(areaCaches[i]->getSize().x);
        int tileH = int(areaCaches[i]->getSize().y);
        if (i < 2) {
            totalW = int(w - 2 * cornerW);
            totalH = tileH;
        } else {
            totalW = tileW;
            totalH = int(h - 2 * cornerH);
        }

        if (totalW <= 0 || totalH <= 0 || tileW <= 0 || tileH <= 0) continue;

        int drawn = 0;
        const bool horizontal = (i < 2);
        const int total = horizontal ? totalW : totalH;
        const int step = horizontal ? tileW : tileH;
        while (drawn < total) {
            int chunk = std::min(step, total - drawn);
            sf::Sprite edgeSprite(*areaCaches[i]);
            if (horizontal) {
                edgeSprite.setTextureRect(sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(chunk, tileH)));
                edgeSprite.setPosition(edgePositions[i] + sf::Vector2f(float(drawn), 0.f));
            } else {
                edgeSprite.setTextureRect(sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(tileW, chunk)));
                edgeSprite.setPosition(edgePositions[i] + sf::Vector2f(0.f, float(drawn)));
            }
            dst.draw(edgeSprite);
            drawn += chunk;
        }
    }
}

void RectBase::renderSides(sf::RenderTexture &edge, const std::vector<sf::Texture *> &cachedCorners,
                           const std::vector<sf::Texture *> &cachedEdges) {
    edge.clear(sf::Color::Transparent);
    auto canvasSize = edge.getSize();
    std::vector<sf::Vector2f> cornerPositions = {
        sf::Vector2f(0, 0), sf::Vector2f(float(canvasSize.x) - float(cachedCorners[1]->getSize().x), 0),
        sf::Vector2f(0, float(canvasSize.y) - float(cachedCorners[2]->getSize().y)),
        sf::Vector2f(float(canvasSize.x) - float(cachedCorners[3]->getSize().x),
                     float(canvasSize.y) - float(cachedCorners[3]->getSize().y))};
    std::vector<sf::Vector2f> edgePositions = {
        sf::Vector2f(float(cachedCorners[0]->getSize().x), 0),
        sf::Vector2f(float(cachedCorners[1]->getSize().x), float(canvasSize.y) - float(cachedCorners[1]->getSize().y)),
        sf::Vector2f(0, float(cachedCorners[2]->getSize().y)),
        sf::Vector2f(float(canvasSize.x) - float(cachedCorners[3]->getSize().x), float(cachedCorners[3]->getSize().y))};
    renderCorners(edge, cachedCorners, cornerPositions);
    renderEdges(edge, cachedEdges, edgePositions);
    edge.display();
}

void RectBase::render(sf::RenderTexture &dst, sf::RenderTexture &edge, sf::Sprite &edgeSprite, sf::Sprite &backSprite,
                      const std::vector<sf::Texture *> &cachedCorners, const std::vector<sf::Texture *> &cachedEdges,
                      sf::RenderStates renderStates) {
    renderSides(edge, cachedCorners, cachedEdges);
    dst.clear(sf::Color::Transparent);
    dst.draw(backSprite, renderStates);
    dst.draw(edgeSprite, renderStates);
    dst.display();
}
