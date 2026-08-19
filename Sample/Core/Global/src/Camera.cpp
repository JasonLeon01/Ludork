#include <Camera.hpp>

#include <Runtime/EngineState.hpp>
#include <System.hpp>
#include <Utils/Math.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

Camera::Camera(std::optional<sf::FloatRect> viewport)
    : viewport_(std::move(viewport)), renderStates_(canvasRenderStates()) {
    if (!viewport_.has_value()) {
        const sf::Vector2u gameSize = System::getGameSize();
        viewport_ = sf::FloatRect(sf::Vector2f(0.0f, 0.0f),
                                  sf::Vector2f(static_cast<float>(gameSize.x),
                                               static_cast<float>(gameSize.y)));
    }
    rebuildRenderTexture(false);
}

void Camera::setViewport(const sf::FloatRect& inViewport) {
    viewport_ = inViewport;
    refreshView();
}

sf::View Camera::getView() const {
    syncDisplayScale();
    return renderTexture_->getView();
}

std::optional<sf::Vector2f> Camera::getViewPosition() const {
    if (!viewport_.has_value()) {
        return std::nullopt;
    }
    return viewport_->position;
}

void Camera::setViewPosition(const sf::Vector2f& inPosition) {
    if (!viewport_.has_value()) {
        return;
    }
    viewport_->position = inPosition;
    refreshView();
}

std::optional<sf::Vector2f> Camera::getViewSize() const {
    if (!viewport_.has_value()) {
        return std::nullopt;
    }
    return viewport_->size;
}

void Camera::setViewSize(const sf::Vector2f& inSize) {
    if (!viewport_.has_value()) {
        return;
    }
    viewport_->size = inSize;
    refreshView();
}

sf::Angle Camera::getViewRotation() const {
    return getView().getRotation();
}

void Camera::setViewRotation(sf::Angle inRotation) {
    syncDisplayScale();
    sf::View view = renderTexture_->getView();
    view.setRotation(inRotation);
    renderTexture_->setView(view);
    defaultViewActive_ = false;
    for (const std::shared_ptr<sf::RenderTexture>& canvas : canvases_) {
        canvas->setView(view);
    }
}

void Camera::moveView(const sf::Vector2f& delta) {
    if (!viewport_.has_value()) {
        return;
    }
    setViewPosition(viewport_->position + delta);
}

void Camera::rotateView(sf::Angle delta) {
    setViewRotation(getViewRotation() + delta);
}

void Camera::resumeViewport() {
    syncDisplayScale();
    renderTexture_->setView(renderTexture_->getDefaultView());
    for (const std::shared_ptr<sf::RenderTexture>& canvas : canvases_) {
        canvas->setView(canvas->getDefaultView());
    }
    defaultViewActive_ = true;
}

sf::Vector2f Camera::getPosition() const {
    return sf::Transformable::getPosition();
}

void Camera::setPosition(const sf::Vector2f& inPosition) {
    sf::Transformable::setPosition(inPosition);
}

void Camera::move(const sf::Vector2f& delta) {
    sf::Transformable::move(delta);
}

sf::Angle Camera::getRotation() const {
    return sf::Transformable::getRotation();
}

void Camera::setRotation(sf::Angle inRotation) {
    sf::Transformable::setRotation(inRotation);
}

void Camera::rotate(sf::Angle delta) {
    sf::Transformable::rotate(delta);
}

sf::Vector2f Camera::getScale() const {
    return sf::Transformable::getScale();
}

void Camera::setScale(const sf::Vector2f& factors) {
    sf::Transformable::setScale(factors);
}

void Camera::scale(const sf::Vector2f& delta) {
    sf::Transformable::scale(delta);
}

std::shared_ptr<Actor> Camera::getParent() const {
    return parent_.lock();
}

void Camera::setParent(std::shared_ptr<Actor> actor) {
    parent_ = ludork::engine::runtime_detail::canonicalRuntimeOwner(actor);
}

void Camera::setMap(std::shared_ptr<GameMapBase> map) {
    map_ = ludork::engine::runtime_detail::canonicalRuntimeOwner(map);
}

std::shared_ptr<GameMapBase> Camera::getMap() const {
    return map_.lock();
}

void Camera::render(const sf::Drawable& object) {
    syncDisplayScale();
    renderTexture_->draw(object, renderStates_);
}

std::optional<sf::FloatRect> Camera::getViewport() const {
    return viewport_;
}

sf::RenderStates Camera::getRenderStates() const {
    return renderStates_;
}

void Camera::setRenderStates(const sf::RenderStates& inRenderStates) {
    renderStates_ = inRenderStates;
}

sf::Vector2f Camera::mapPixelToCoords(const sf::Vector2i& point) const {
    syncDisplayScale();
    const float scale = displayScale();
    const sf::Vector2i scaled(
        static_cast<int>(roundNumber(static_cast<double>(point.x) * scale)),
        static_cast<int>(roundNumber(static_cast<double>(point.y) * scale)));
    return renderTexture_->mapPixelToCoords(scaled, renderTexture_->getView());
}

sf::Vector2i Camera::mapCoordsToPixel(const sf::Vector2f& point) const {
    syncDisplayScale();
    const float scale = displayScale();
    const sf::Vector2i scaled =
        renderTexture_->mapCoordsToPixel(point, renderTexture_->getView());
    return {
        static_cast<int>(roundNumber(static_cast<double>(scaled.x) / scale)),
        static_cast<int>(roundNumber(static_cast<double>(scaled.y) / scale)),
    };
}

const sf::Texture& Camera::getTexture() const {
    syncDisplayScale();
    return renderTexture_->getTexture();
}

std::shared_ptr<sf::RenderTexture> Camera::getRenderTexture() const {
    syncDisplayScale();
    return renderTexture_;
}

sf::Image Camera::getImage() const {
    syncDisplayScale();
    return renderTexture_->getTexture().copyToImage();
}

void Camera::onTick(float) {}

void Camera::onLateTick(float) {}

void Camera::onFixedTick(float) {
    syncFollowTarget();
}

void Camera::syncFollowTarget() {
    syncDisplayScale();
    if (map_.expired() || !viewport_.has_value()) {
        return;
    }
    const std::shared_ptr<Actor> parent = parent_.lock();
    if (parent == nullptr) {
        return;
    }
    const sf::Vector2f parentPosition = parent->getPosition();
    setViewPosition({parentPosition.x - viewport_->size.x / 2.0f,
                     parentPosition.y - viewport_->size.y / 2.0f});
    fixViewPosition();
}

void Camera::fixViewPosition() {
    const std::shared_ptr<GameMapBase> map = map_.lock();
    if (map == nullptr || !viewport_.has_value()) {
        return;
    }
    const sf::Vector2u mapSize = map->getSize();
    const float maxX = std::max(
        0.0f, static_cast<float>(mapSize.x * CellSize) - viewport_->size.x);
    const float maxY = std::max(
        0.0f, static_cast<float>(mapSize.y * CellSize) - viewport_->size.y);
    setViewPosition({std::clamp(viewport_->position.x, 0.0f, maxX),
                     std::clamp(viewport_->position.y, 0.0f, maxY)});
}

void Camera::clear() {
    syncDisplayScale();
    renderTexture_->clear(sf::Color::Transparent);
}

void Camera::display() {
    syncDisplayScale();
    renderTexture_->display();
}

void Camera::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    syncDisplayScale();
    states.transform.combine(getTransform());
    target.draw(*renderSprite_, states);
}

float Camera::displayScale() const {
    const float scale = System::getScale();
    return scale > 0.0f ? scale : 1.0f;
}

sf::Vector2u Camera::renderPixelSize() const {
    const float scale = displayScale();
    const float width = std::max(1.0f, viewport_->size.x * scale);
    const float height = std::max(1.0f, viewport_->size.y * scale);
    return {static_cast<unsigned int>(width),
            static_cast<unsigned int>(height)};
}

void Camera::rebuildRenderTexture(bool preserveView) const {
    const sf::Vector2u size = renderPixelSize();
    std::optional<sf::View> preservedView;
    std::vector<std::optional<sf::View>> preservedCanvasViews;
    if (renderTexture_ == nullptr) {
        renderTexture_ = std::make_shared<sf::RenderTexture>(size);
    } else {
        if (preserveView && !defaultViewActive_) {
            preservedView = renderTexture_->getView();
        }
        if (renderTexture_->getSize() != size &&
            !renderTexture_->resize(size)) {
            throw std::runtime_error("Failed to resize Camera render target");
        }
    }
    renderTexture_->setSmooth(false);
    renderTexture_->setView(defaultViewActive_
                                ? renderTexture_->getDefaultView()
                                : preservedView.value_or(sf::View(*viewport_)));
    renderTexture_->clear(sf::Color::Transparent);
    renderTexture_->display();
    if (renderSprite_.has_value()) {
        renderSprite_->setTexture(renderTexture_->getTexture(), true);
    } else {
        renderSprite_.emplace(renderTexture_->getTexture());
    }
    const float scale = displayScale();
    renderSprite_->setScale({1.0f / scale, 1.0f / scale});
    preservedCanvasViews.reserve(canvases_.size());
    for (std::shared_ptr<sf::RenderTexture>& canvas : canvases_) {
        if (preserveView && !defaultViewActive_) {
            preservedCanvasViews.emplace_back(canvas->getView());
        } else {
            preservedCanvasViews.emplace_back(std::nullopt);
        }
        if (canvas->getSize() != size) {
            if (!canvas->resize(size)) {
                throw std::runtime_error(
                    "Failed to resize Camera auxiliary render target");
            }
        }
        canvas->setSmooth(false);
        canvas->setView(
            defaultViewActive_
                ? canvas->getDefaultView()
                : preservedCanvasViews.back().value_or(sf::View(*viewport_)));
        canvas->clear(sf::Color::Transparent);
        canvas->display();
    }
}

void Camera::syncDisplayScale() const {
    if (!viewport_.has_value()) {
        return;
    }
    if (renderTexture_ == nullptr ||
        renderTexture_->getSize() != renderPixelSize()) {
        rebuildRenderTexture(true);
        return;
    }
    const float scale = displayScale();
    renderSprite_->setScale({1.0f / scale, 1.0f / scale});
}

void Camera::refreshView() {
    if (!viewport_.has_value()) {
        return;
    }
    sf::View view = renderTexture_ == nullptr ? sf::View(*viewport_)
                                              : renderTexture_->getView();
    view.setCenter(viewport_->getCenter());
    view.setSize(viewport_->size);
    defaultViewActive_ = false;
    if (renderTexture_ == nullptr ||
        renderTexture_->getSize() != renderPixelSize()) {
        rebuildRenderTexture(true);
    }
    renderTexture_->setView(view);
    const float scale = displayScale();
    renderSprite_->setScale({1.0f / scale, 1.0f / scale});
    for (const std::shared_ptr<sf::RenderTexture>& canvas : canvases_) {
        canvas->setView(view);
    }
}
