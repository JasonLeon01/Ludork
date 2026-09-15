#include <UI/Image.hpp>

#include <EngineState.hpp>

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

Image::Image(std::shared_ptr<sf::Texture> texture,
             std::optional<sf::IntRect> rect)
    : SpriteBase(std::move(texture), rect) {}

Image::DrawAs Image::getDrawAs() const {
    return drawAs_;
}

void Image::setDrawAs(DrawAs drawAs) {
    if (drawAs != DrawAs::Image && drawAs != DrawAs::Tile) {
        throw std::invalid_argument("Unknown Image DrawAs value");
    }
    if (drawAs_ == drawAs) {
        return;
    }
    drawAs_ = drawAs;
    tileTexture_.reset();
}

void Image::onTextureChanged() {
    tileTexture_.reset();
}

void Image::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (drawAs_ == DrawAs::Image) {
        SpriteBase::draw(target, states);
        return;
    }

    const sf::Vector2f scale = getScale();
    const sf::Vector2i rectSize = getTextureRect().size;
    if (rectSize.x == 0 || rectSize.y == 0 || scale.x == 0.0f ||
        scale.y == 0.0f || getTexture().getSize().x == 0 ||
        getTexture().getSize().y == 0) {
        return;
    }
    prepareTileTexture();

    const sf::Vector2f size{std::abs(static_cast<float>(rectSize.x)),
                            std::abs(static_cast<float>(rectSize.y))};
    const sf::Vector2f extent{size.x * std::abs(scale.x),
                              size.y * std::abs(scale.y)};
    const sf::Color colour = presentedColour();
    const std::array<sf::Vertex, 4> vertices{{
        {{0.0f, 0.0f}, colour, {0.0f, 0.0f}},
        {{0.0f, size.y}, colour, {0.0f, extent.y}},
        {{size.x, 0.0f}, colour, {extent.x, 0.0f}},
        {size, colour, extent},
    }};
    _applyRenderStates(states);
    states.blendMode = getRenderStates().blendMode;
    states.texture = tileTexture_.get();
    states.coordinateType = sf::CoordinateType::Pixels;
    target.draw(vertices.data(), vertices.size(),
                sf::PrimitiveType::TriangleStrip, states);
}

void Image::prepareTileTexture() const {
    if (tileTexture_ != nullptr) {
        return;
    }
    const sf::Texture& source = getTexture();
    const sf::IntRect rect = getTextureRect();
    const std::int64_t width = std::abs(static_cast<std::int64_t>(rect.size.x));
    const std::int64_t height =
        std::abs(static_cast<std::int64_t>(rect.size.y));
    const unsigned int maximumSize = sf::Texture::getMaximumSize();
    if (width > maximumSize || height > maximumSize) {
        throw std::invalid_argument(
            "Image tile exceeds the maximum texture size");
    }
    const sf::Vector2u size{static_cast<unsigned int>(width),
                            static_cast<unsigned int>(height)};
    if (rect.position == sf::Vector2i{} && rect.size.x > 0 && rect.size.y > 0 &&
        size == source.getSize()) {
        tileTexture_ = std::make_unique<sf::Texture>(source);
    } else {
        sf::ContextSettings settings;
        settings.sRgbCapable = source.isSrgb();
        sf::RenderTexture tile(size, settings);
        tile.clear(sf::Color::Transparent);
        tile.draw(sf::Sprite(source, rect), sf::RenderStates(sf::BlendNone));
        tile.display();
        tileTexture_ = std::make_unique<sf::Texture>(tile.getTexture());
    }
    if (tileTexture_->getSize() != size) {
        tileTexture_.reset();
        throw std::runtime_error("Failed to create Image tile texture");
    }
    tileTexture_->setSmooth(source.isSmooth());
    tileTexture_->setRepeated(true);
}

void Image::_applyRenderStates(sf::RenderStates& states) const {
    SpriteBase::_applyRenderStates(states);
    states.transform.scale(
        {engineState().getScale(), engineState().getScale()});
}

sf::Transform Image::_getRenderTransform() const {
    sf::Transform transform = SpriteBase::_getRenderTransform();
    transform.scale({engineState().getScale(), engineState().getScale()});
    return transform;
}
