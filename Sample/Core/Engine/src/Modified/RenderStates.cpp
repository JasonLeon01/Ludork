#include <Modified/RenderStates.hpp>

#include <SFML/Graphics/BlendMode.hpp>

namespace {

sf::BlendMode canvasBlendMode() {
    return sf::BlendMode(
        sf::BlendMode::Factor::SrcAlpha,
        sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add,
        sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcAlpha,
        sf::BlendMode::Equation::Add);
}

}  // namespace

ModifiedRenderStates::ModifiedRenderStates(const sf::RenderStates& states)
    : sf::RenderStates(states) {}

ModifiedRenderStates ModifiedRenderStates::Default() {
    return ModifiedRenderStates(sf::RenderStates(canvasBlendMode()));
}
