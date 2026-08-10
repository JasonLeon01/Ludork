#include <Utils/Render.hpp>

#include <SFML/Graphics/BlendMode.hpp>

#include <algorithm>

sf::RenderStates canvasRenderStates() {
    const sf::BlendMode blend(
        sf::BlendMode::Factor::SrcAlpha,
        sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add,
        sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcAlpha,
        sf::BlendMode::Equation::Add);
    return sf::RenderStates(blend);
}

sf::RenderStates premultipliedRenderStates() {
    const sf::BlendMode blend(
        sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcAlpha,
        sf::BlendMode::Equation::Add, sf::BlendMode::Factor::One,
        sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add);
    return sf::RenderStates(blend);
}

sf::Vector2u nonZeroRenderTextureSize(const sf::Vector2u& size) {
    return {std::max(1u, size.x), std::max(1u, size.y)};
}
