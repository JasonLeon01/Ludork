#include <UI/SolidRect.hpp>

#include <Runtime/EngineState.hpp>

SolidRect::SolidRect(const sf::Vector2f& size, const sf::Color& fillColor,
                     const sf::Color& outlineColor, float outlineThickness)
    : size_(size), shape_(size * Scale), outlineThickness_(outlineThickness) {
    shape_.setFillColor(fillColor);
    shape_.setOutlineColor(outlineColor);
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
    return shape_.getFillColor();
}

void SolidRect::setFillColor(const sf::Color& color) {
    shape_.setFillColor(color);
}

sf::Color SolidRect::getOutlineColor() const {
    return shape_.getOutlineColor();
}

void SolidRect::setOutlineColor(const sf::Color& color) {
    shape_.setOutlineColor(color);
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
