#include <Utils/Render.hpp>

#include <SFML/Graphics/BlendMode.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

sf::RenderStates canvasRenderStates() {
    const sf::BlendMode blend(
        sf::BlendMode::Factor::SrcAlpha,
        sf::BlendMode::Factor::OneMinusSrcAlpha, sf::BlendMode::Equation::Add,
        sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcAlpha,
        sf::BlendMode::Equation::Add);
    return sf::RenderStates(blend);
}

sf::VertexArray buildPixelGridVertices(const sf::Vector2f& origin,
                                       const sf::Vector2u& size) {
    if (size.x == 0 || size.y == 0) {
        return sf::VertexArray(sf::PrimitiveType::Triangles);
    }
    const std::size_t width = size.x;
    const std::size_t height = size.y;
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    if (width > maximum / height || width * height > maximum / 6) {
        throw std::length_error("Pixel grid vertex count is too large");
    }
    sf::VertexArray vertices(sf::PrimitiveType::Triangles, width * height * 6);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const float left = origin.x + static_cast<float>(x);
            const float top = origin.y + static_cast<float>(y);
            const float right = left + 1.0f;
            const float bottom = top + 1.0f;
            const sf::Vector2f textureCoordinate(static_cast<float>(x) + 0.5f,
                                                 static_cast<float>(y) + 0.5f);
            const std::size_t base = (y * width + x) * 6;
            vertices[base + 0] = {
                {left, top}, sf::Color::White, textureCoordinate};
            vertices[base + 1] = {
                {right, top}, sf::Color::White, textureCoordinate};
            vertices[base + 2] = {
                {right, bottom}, sf::Color::White, textureCoordinate};
            vertices[base + 3] = {
                {left, top}, sf::Color::White, textureCoordinate};
            vertices[base + 4] = {
                {right, bottom}, sf::Color::White, textureCoordinate};
            vertices[base + 5] = {
                {left, bottom}, sf::Color::White, textureCoordinate};
        }
    }
    return vertices;
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
