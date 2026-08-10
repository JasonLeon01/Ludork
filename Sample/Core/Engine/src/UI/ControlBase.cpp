#include <UI/ControlBase.hpp>

#include <Runtime/EngineState.hpp>
#include <Utils/Render.hpp>

bool ControlBase::getVisible() const {
    return visible_;
}

void ControlBase::setVisible(bool visible) {
    visible_ = visible;
}

const std::string& ControlBase::getName() const {
    return name_;
}

void ControlBase::setName(const std::string& name) {
    name_ = name;
}

std::shared_ptr<ControlBase> ControlBase::getParent() const {
    return parent_.lock();
}

void ControlBase::setParent(const std::shared_ptr<ControlBase>& parent) {
    parent_ = parent;
}

std::vector<std::shared_ptr<ControlBase>> ControlBase::getChildren() const {
    return {};
}

sf::Vector2f ControlBase::getSize() const {
    return {0.0f, 0.0f};
}

sf::FloatRect ControlBase::getLocalBounds() const {
    return {{0.0f, 0.0f}, getSize()};
}

sf::FloatRect ControlBase::getAbsoluteBounds() const {
    const sf::FloatRect bounds = getLocalBounds();
    const sf::FloatRect scaledBounds(bounds.position * Scale,
                                     bounds.size * Scale);
    return _getScreenRenderTransform().transformRect(scaledBounds);
}

sf::RenderStates ControlBase::getRenderStates() const {
    return canvasRenderStates();
}

void ControlBase::setPosition(const sf::Vector2f& position) {
    sf::Transformable::setPosition(position);
}

sf::Vector2f ControlBase::getPosition() const {
    return sf::Transformable::getPosition();
}

void ControlBase::move(const sf::Vector2f& offset) {
    sf::Transformable::move(offset);
}

sf::Angle ControlBase::getRotation() const {
    return sf::Transformable::getRotation();
}

void ControlBase::setRotation(sf::Angle angle) {
    sf::Transformable::setRotation(angle);
}

void ControlBase::setRotationDegrees(float angle) {
    sf::Transformable::setRotation(sf::degrees(angle));
}

void ControlBase::rotate(sf::Angle angle) {
    sf::Transformable::rotate(angle);
}

void ControlBase::rotateDegrees(float angle) {
    sf::Transformable::rotate(sf::degrees(angle));
}

sf::Vector2f ControlBase::getScale() const {
    return sf::Transformable::getScale();
}

void ControlBase::setScale(const sf::Vector2f& scale) {
    sf::Transformable::setScale(scale);
}

void ControlBase::scale(const sf::Vector2f& factor) {
    sf::Transformable::scale(factor);
}

sf::Vector2f ControlBase::getOrigin() const {
    return sf::Transformable::getOrigin();
}

void ControlBase::setOrigin(const sf::Vector2f& origin) {
    sf::Transformable::setOrigin(origin);
}

sf::Transform ControlBase::getTransform() const {
    return sf::Transformable::getTransform();
}

sf::Transform ControlBase::getInverseTransform() const {
    return sf::Transformable::getInverseTransform();
}

sf::Transform ControlBase::renderTransform() const {
    return _getRenderTransform();
}

sf::Transform ControlBase::screenRenderTransform() const {
    return _getScreenRenderTransform();
}

sf::Transform ControlBase::_getScreenTransform() const {
    sf::Transform transform = getTransform();
    const std::shared_ptr<ControlBase> parent = getParent();
    if (parent != nullptr) {
        transform = parent->_getScreenTransform() * transform;
    }
    return transform;
}

void ControlBase::_applyRenderStates(sf::RenderStates& states) const {
    states.transform.translate(getPosition() * (Scale - 1.0f));
    states.transform.combine(getTransform());
}

sf::Transform ControlBase::_getRenderTransform() const {
    sf::Transform transform;
    transform.translate(getPosition() * (Scale - 1.0f));
    transform.combine(getTransform());
    return transform;
}

sf::Transform ControlBase::_getScreenRenderTransform() const {
    sf::Transform transform = _getRenderTransform();
    const std::shared_ptr<ControlBase> parent = getParent();
    if (parent != nullptr) {
        transform = parent->_getScreenRenderTransform() * transform;
    }
    return transform;
}

void ControlBase::draw(sf::RenderTarget&, sf::RenderStates) const {}
