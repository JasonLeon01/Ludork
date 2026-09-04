#include <UI/SolidRect.hpp>

#include <EngineState.hpp>

#include <algorithm>
#include <cstdint>

SolidRect::SolidRect(const sf::Vector2f& size, const sf::Color& fillColor,
                     const sf::Color& outlineColor, float outlineThickness)
    : size_(size), shape_(size * Scale), outlineThickness_(outlineThickness) {
    fillColor_ = fillColor;
    outlineColor_ = outlineColor;
    applyColours();
    shape_.setOutlineThickness(outlineThickness * Scale);
}

sf::Vector2f SolidRect::getSize() const {
    return size_;
}

void SolidRect::setSize(const sf::Vector2f& size) {
    size_ = size;
    shape_.setSize(size_ * Scale);
}

sf::Color SolidRect::getFillColor() const {
    return fillColor_;
}

void SolidRect::setFillColor(const sf::Color& color) {
    fillColor_ = color;
    applyColours();
}

sf::Color SolidRect::getOutlineColor() const {
    return outlineColor_;
}

void SolidRect::setOutlineColor(const sf::Color& color) {
    outlineColor_ = color;
    applyColours();
}

float SolidRect::getOutlineThickness() const {
    return outlineThickness_;
}

void SolidRect::setOutlineThickness(float thickness) {
    outlineThickness_ = thickness;
    shape_.setOutlineThickness(thickness * Scale);
}

sf::FloatRect SolidRect::getLocalBounds() const {
    const sf::FloatRect bounds = shape_.getLocalBounds();
    return {bounds.position, bounds.size / Scale};
}

sf::FloatRect SolidRect::getGlobalBounds() const {
    const sf::FloatRect bounds = shape_.getGlobalBounds();
    return {bounds.position, bounds.size / Scale};
}

void SolidRect::refreshDisplayScale() {
    shape_.setSize(size_ * Scale);
    shape_.setOutlineThickness(outlineThickness_ * Scale);
    ControlBase::refreshDisplayScale();
}

void SolidRect::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    if (getVisible()) {
        target.draw(shape_, states);
    }
}

void SolidRect::_refreshPresentationColour() {
    applyColours();
}

void SolidRect::applyColours() {
    shape_.setFillColor(modulatePresentationColour(fillColor_));
    shape_.setOutlineColor(modulatePresentationColour(outlineColor_));
}
