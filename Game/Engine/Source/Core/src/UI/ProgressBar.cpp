#include <UI/ProgressBar.hpp>

#include <EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
void updateQuad(std::array<sf::Vertex, 4>& vertices, const sf::Vector2f& size,
                const std::shared_ptr<sf::Texture>& texture,
                const std::optional<sf::IntRect>& textureRect, float progress) {
    const sf::FloatRect source =
        textureRect.has_value()
            ? sf::FloatRect(*textureRect)
            : sf::FloatRect({0.0f, 0.0f}, texture != nullptr
                                              ? sf::Vector2f(texture->getSize())
                                              : sf::Vector2f{});
    const float width = size.x * progress;
    const float right = source.position.x + source.size.x * progress;
    const float bottom = source.position.y + source.size.y;
    vertices[0].position = {0.0f, 0.0f};
    vertices[1].position = {width, 0.0f};
    vertices[2].position = {0.0f, size.y};
    vertices[3].position = {width, size.y};
    vertices[0].texCoords = source.position;
    vertices[1].texCoords = {right, source.position.y};
    vertices[2].texCoords = {source.position.x, bottom};
    vertices[3].texCoords = {right, bottom};
}
}  // namespace

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

std::shared_ptr<sf::Texture> ProgressBar::getBackgroundTexture() const {
    return backgroundTexture_;
}

void ProgressBar::setBackgroundTexture(std::shared_ptr<sf::Texture> texture) {
    backgroundTexture_ = std::move(texture);
    updateGeometry();
}

std::shared_ptr<sf::Texture> ProgressBar::getFillTexture() const {
    return fillTexture_;
}

void ProgressBar::setFillTexture(std::shared_ptr<sf::Texture> texture) {
    fillTexture_ = std::move(texture);
    updateGeometry();
}

std::optional<sf::IntRect> ProgressBar::getBackgroundTextureRect() const {
    return backgroundTextureRect_;
}

void ProgressBar::setBackgroundTextureRect(std::optional<sf::IntRect> rect) {
    backgroundTextureRect_ = rect;
    updateGeometry();
}

std::optional<sf::IntRect> ProgressBar::getFillTextureRect() const {
    return fillTextureRect_;
}

void ProgressBar::setFillTextureRect(std::optional<sf::IntRect> rect) {
    fillTextureRect_ = rect;
    updateGeometry();
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
    states.coordinateType = sf::CoordinateType::Pixels;
    states.texture = backgroundTexture_.get();
    target.draw(background_.data(), background_.size(),
                sf::PrimitiveType::TriangleStrip, states);
    states.texture = fillTexture_.get();
    target.draw(fill_.data(), fill_.size(), sf::PrimitiveType::TriangleStrip,
                states);
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
    const sf::Vector2f size = size_ * engineState().getScale();
    updateQuad(background_, size, backgroundTexture_, backgroundTextureRect_,
               1.0f);
    updateQuad(fill_, size, fillTexture_, fillTextureRect_, progress_);
}

void ProgressBar::refreshDisplayScale() {
    updateGeometry();
    ControlBase::refreshDisplayScale();
}

void ProgressBar::_refreshPresentationColour() {
    applyColours();
}

void ProgressBar::applyColours() {
    for (sf::Vertex& vertex : background_) {
        vertex.color = modulatePresentationColour(backgroundColor_);
    }
    for (sf::Vertex& vertex : fill_) {
        vertex.color = modulatePresentationColour(fillColor_);
    }
}
