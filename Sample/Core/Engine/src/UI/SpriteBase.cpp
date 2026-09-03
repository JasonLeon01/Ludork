#include <UI/SpriteBase.hpp>

#include <Runtime/EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {

std::uint8_t premultipliedChannel(std::uint8_t channel, std::uint8_t alpha) {
    const unsigned int product =
        static_cast<unsigned int>(channel) * alpha + 127u;
    return static_cast<std::uint8_t>(product / 255u);
}

sf::Color premultipliedColour(const sf::Color& colour) {
    return {
        premultipliedChannel(colour.r, colour.a),
        premultipliedChannel(colour.g, colour.a),
        premultipliedChannel(colour.b, colour.a),
        colour.a,
    };
}

}  // namespace

SpriteBase::SpriteBase(std::shared_ptr<sf::Texture> texture,
                       std::optional<sf::IntRect> rect)
    : texture_(std::move(texture)), renderStates_(canvasRenderStates()) {
    if (!texture_) {
        throw std::invalid_argument("SpriteBase texture must not be null");
    }
    sprite_ = rect.has_value() ? std::make_unique<sf::Sprite>(*texture_, *rect)
                               : std::make_unique<sf::Sprite>(*texture_);
}

void SpriteBase::setTexture(std::shared_ptr<sf::Texture> texture,
                            bool resetRect) {
    if (!texture) {
        throw std::invalid_argument("SpriteBase texture must not be null");
    }
    texture_ = std::move(texture);
    sprite_->setTexture(*texture_, resetRect);
}

const sf::Texture& SpriteBase::getTexture() const {
    return sprite_->getTexture();
}

void SpriteBase::setTextureRect(const sf::IntRect& rect) {
    sprite_->setTextureRect(rect);
}

sf::IntRect SpriteBase::getTextureRect() const {
    return sprite_->getTextureRect();
}

void SpriteBase::setColour(const sf::Color& colour) {
    colour_ = colour;
    applyColour();
}

sf::Color SpriteBase::getColour() const {
    return colour_;
}

sf::Vector2f SpriteBase::getSize() const {
    return sprite_->getGlobalBounds().size;
}

sf::FloatRect SpriteBase::getLocalBounds() const {
    const sf::FloatRect bounds = sprite_->getLocalBounds();
    return {bounds.position, bounds.size / Scale};
}

sf::FloatRect SpriteBase::getGlobalBounds() const {
    const sf::FloatRect bounds = sprite_->getGlobalBounds();
    return {bounds.position, bounds.size / Scale};
}

sf::RenderStates SpriteBase::getRenderStates() const {
    return renderStates_;
}

void SpriteBase::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    states.blendMode = renderStates_.blendMode;
    target.draw(*sprite_, states);
}

void SpriteBase::setPremultipliedTexture(bool premultiplied) {
    premultipliedTexture_ = premultiplied;
    renderStates_ =
        premultiplied ? premultipliedRenderStates() : canvasRenderStates();
    applyColour();
}

sf::Color SpriteBase::presentedColour() const {
    return modulatePresentationColour(colour_);
}

void SpriteBase::_refreshPresentationColour() {
    applyColour();
}

void SpriteBase::applyColour() {
    const sf::Color colour = presentedColour();
    sprite_->setColor(premultipliedTexture_ ? premultipliedColour(colour)
                                            : colour);
}

void SpriteBase::_applyRenderStates(sf::RenderStates& states) const {
    ControlBase::_applyRenderStates(states);
}

sf::Transform SpriteBase::_getRenderTransform() const {
    return ControlBase::_getRenderTransform();
}
