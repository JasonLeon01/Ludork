#include <UI/ProgressBar.hpp>

#include <Runtime/EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

ProgressBar::ProgressBar(const sf::Vector2f& size, float progress,
                         const sf::Color& backgroundColor,
                         const sf::Color& fillColor)
    : size_(normalizedSize(size)), progress_(normalizedProgress(progress)) {
    backgroundColor_ = backgroundColor;
    fillColor_ = fillColor;
    applyColours();
    updateGeometry();
}

sf::Vector2f ProgressBar::getSize() const {
    return size_;
}

void ProgressBar::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (size_ == normalized) {
        return;
    }
    size_ = normalized;
    updateGeometry();
}

float ProgressBar::getProgress() const {
    return progress_;
}

void ProgressBar::setProgress(float progress) {
    const float normalized = normalizedProgress(progress);
    if (progress_ == normalized) {
        return;
    }
    progress_ = normalized;
    updateGeometry();
}

sf::Color ProgressBar::getBackgroundColor() const {
    return backgroundColor_;
}

void ProgressBar::setBackgroundColor(const sf::Color& color) {
    backgroundColor_ = color;
    applyColours();
}

sf::Color ProgressBar::getFillColor() const {
    return fillColor_;
}

void ProgressBar::setFillColor(const sf::Color& color) {
    fillColor_ = color;
    applyColours();
}

sf::FloatRect ProgressBar::getLocalBounds() const {
    return {{0.0f, 0.0f}, size_};
}

void ProgressBar::draw(sf::RenderTarget& target,
                       sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    target.draw(background_, states);
    target.draw(fill_, states);
}

sf::Vector2f ProgressBar::normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::max(0.0f, size.x) : 0.0f,
        std::isfinite(size.y) ? std::max(0.0f, size.y) : 0.0f,
    };
}

float ProgressBar::normalizedProgress(float progress) {
    return std::isfinite(progress) ? std::clamp(progress, 0.0f, 1.0f) : 0.0f;
}

void ProgressBar::updateGeometry() {
    background_.setSize(size_ * Scale);
    fill_.setSize({size_.x * progress_ * Scale, size_.y * Scale});
}

void ProgressBar::refreshDisplayScale() {
    updateGeometry();
    ControlBase::refreshDisplayScale();
}

void ProgressBar::_refreshPresentationColour() {
    applyColours();
}

void ProgressBar::applyColours() {
    background_.setFillColor(modulatePresentationColour(backgroundColor_));
    fill_.setFillColor(modulatePresentationColour(fillColor_));
}
