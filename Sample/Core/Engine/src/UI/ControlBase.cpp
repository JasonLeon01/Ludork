#include <UI/ControlBase.hpp>

#include <Runtime/EngineState.hpp>
#include <UI/FunctionalBase.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {

std::uint8_t normalizedColourComponent(float value) {
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(value * 255.0f), 0L, 255L));
}

sf::Color combinePresentationColours(
    const std::unordered_map<const void*, sf::Color>& colours) {
    std::array<float, 4> combined{1.0f, 1.0f, 1.0f, 1.0f};
    for (const auto& [source, colour] : colours) {
        static_cast<void>(source);
        combined[0] *= static_cast<float>(colour.r) / 255.0f;
        combined[1] *= static_cast<float>(colour.g) / 255.0f;
        combined[2] *= static_cast<float>(colour.b) / 255.0f;
        combined[3] *= static_cast<float>(colour.a) / 255.0f;
    }
    return {normalizedColourComponent(combined[0]),
            normalizedColourComponent(combined[1]),
            normalizedColourComponent(combined[2]),
            normalizedColourComponent(combined[3])};
}

std::uint8_t modulateColourComponent(std::uint8_t authored,
                                     std::uint8_t presentation) {
    return static_cast<std::uint8_t>(
        (static_cast<unsigned int>(authored) * presentation + 127U) / 255U);
}

}  // namespace

std::weak_ptr<RuntimeCallbackRegistry>
    ControlBase::activeRuntimeCallbackRegistry_;

void RuntimeCallbackRegistry::registerControl(ControlBase* control) {
    if (control == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    controls_.insert(control);
}

void RuntimeCallbackRegistry::unregisterControl(ControlBase* control) noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    controls_.erase(control);
}

void RuntimeCallbackRegistry::releaseRuntimeCallbacks() noexcept {
    std::vector<ControlBase*> controls;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        controls.assign(controls_.begin(), controls_.end());
    }
    for (ControlBase* control : controls) {
        if (!contains(control)) {
            continue;
        }
        const std::shared_ptr<ControlBase> owner =
            control->weak_from_this().lock();
        if (owner != nullptr && owner->getParent() == nullptr) {
            owner->releaseRuntimeCallbacks();
        }
    }
}

bool RuntimeCallbackRegistry::contains(ControlBase* control) const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return controls_.contains(control);
}

ControlBase::ControlBase() {
    adoptRuntimeCallbackRegistry(activeRuntimeCallbackRegistry_.lock());
}

ControlBase::~ControlBase() {
    const std::shared_ptr<RuntimeCallbackRegistry> registry =
        runtimeCallbackRegistry_.lock();
    if (registry != nullptr) {
        registry->unregisterControl(this);
    }
}

bool ControlBase::getVisible() const {
    return visible_;
}

void ControlBase::setVisible(bool visible) {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    if (!visible_) {
        resetFunctionalInteractions(*this);
        if (presentationRelease_) {
            presentationRelease_();
        }
    }
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
    if (parent_.lock() == parent) {
        return;
    }
    resetFunctionalInteractions(*this);
    parent_ = parent;
    if (parent != nullptr) {
        const std::shared_ptr<RuntimeCallbackRegistry> registry =
            parent->runtimeCallbackRegistry_.lock();
        if (registry != nullptr) {
            adoptRuntimeCallbackRegistry(registry);
        }
    }
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

sf::FloatRect ControlBase::getContentBounds() const {
    return getLocalBounds();
}

sf::FloatRect ControlBase::getAbsoluteInteractionBounds() const {
    return clipAbsoluteInteractionBounds(getAbsoluteBounds());
}

sf::FloatRect ControlBase::clipAbsoluteInteractionBounds(
    const sf::FloatRect& bounds) const {
    if (_ignoresAncestorInteractionClip()) {
        return bounds;
    }
    sf::FloatRect clipped = bounds;
    std::shared_ptr<ControlBase> parent = getParent();
    while (parent != nullptr) {
        const std::optional<sf::FloatRect> clip =
            parent->_getAbsoluteChildInteractionClipBounds();
        if (clip.has_value()) {
            const std::optional<sf::FloatRect> intersection =
                clipped.findIntersection(*clip);
            if (!intersection.has_value()) {
                return {{0.0f, 0.0f}, {0.0f, 0.0f}};
            }
            clipped = *intersection;
        }
        parent = parent->getParent();
    }
    return clipped;
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

void ControlBase::setPresentationTransform(const sf::Vector2f& translation,
                                           float rotation,
                                           const sf::Vector2f& scale,
                                           const sf::Vector2f& pivot) {
    presentationTranslation_ = translation;
    presentationRotation_ = rotation;
    presentationScale_ = scale;
    presentationPivot_ = pivot;
}

void ControlBase::resetPresentationTransform() {
    presentationTranslation_ = {0.0f, 0.0f};
    presentationRotation_ = 0.0f;
    presentationScale_ = {1.0f, 1.0f};
    presentationPivot_ = {0.5f, 0.5f};
}

void ControlBase::setPresentationColour(const void* source,
                                        const sf::Color& colour) {
    if (source == nullptr) {
        return;
    }
    presentationColours_.insert_or_assign(source, colour);
    const sf::Color combined = combinePresentationColours(presentationColours_);
    if (presentationColour_ != combined) {
        presentationColour_ = combined;
        _refreshPresentationColour();
    }
}

void ControlBase::clearPresentationColour(const void* source) {
    if (source == nullptr || presentationColours_.erase(source) == 0) {
        return;
    }
    const sf::Color combined = combinePresentationColours(presentationColours_);
    if (presentationColour_ != combined) {
        presentationColour_ = combined;
        _refreshPresentationColour();
    }
}

void ControlBase::setPresentationUpdater(std::function<void(float)> updater) {
    presentationUpdater_ = std::move(updater);
}

void ControlBase::setPresentationRelease(std::function<void()> release) {
    presentationRelease_ = std::move(release);
}

void ControlBase::updatePresentationAnimations(float deltaTime) {
    if (presentationUpdater_) {
        presentationUpdater_(deltaTime);
    }
}

void ControlBase::refreshDisplayScale() {
    for (const std::shared_ptr<ControlBase>& child : getChildren()) {
        if (child != nullptr) {
            child->refreshDisplayScale();
        }
    }
}

void ControlBase::releaseRuntimeCallbacks() noexcept {
    if (presentationRelease_) {
        presentationRelease_();
    }
    if (FunctionalBase* functional = dynamic_cast<FunctionalBase*>(this)) {
        functional->clearEventCallbacks();
    }
    for (const std::shared_ptr<ControlBase>& child : getChildren()) {
        if (child != nullptr) {
            child->releaseRuntimeCallbacks();
        }
    }
}

void ControlBase::adoptRuntimeCallbackRegistry(
    const std::shared_ptr<RuntimeCallbackRegistry>& registry) {
    const std::shared_ptr<RuntimeCallbackRegistry> current =
        runtimeCallbackRegistry_.lock();
    if (current != registry) {
        if (current != nullptr) {
            current->unregisterControl(this);
        }
        runtimeCallbackRegistry_ = registry;
        if (registry != nullptr) {
            registry->registerControl(this);
        }
    }
    for (const std::shared_ptr<ControlBase>& child : getChildren()) {
        if (child != nullptr) {
            child->adoptRuntimeCallbackRegistry(registry);
        }
    }
}

void ControlBase::activateRuntimeCallbackRegistry(
    const std::shared_ptr<RuntimeCallbackRegistry>& registry) noexcept {
    activeRuntimeCallbackRegistry_ = registry;
}

void ControlBase::deactivateRuntimeCallbackRegistry(
    const std::shared_ptr<RuntimeCallbackRegistry>& registry) noexcept {
    if (activeRuntimeCallbackRegistry_.lock() == registry) {
        activeRuntimeCallbackRegistry_.reset();
    }
}

void ControlBase::resetActiveRuntimeCallbackRegistry() noexcept {
    activeRuntimeCallbackRegistry_.reset();
}

void ControlBase::resetFunctionalInteractions(ControlBase& control) {
    if (FunctionalBase* functional = dynamic_cast<FunctionalBase*>(&control)) {
        functional->resetPointerInteraction();
    }
    for (const std::shared_ptr<ControlBase>& child : control.getChildren()) {
        if (child != nullptr) {
            resetFunctionalInteractions(*child);
        }
    }
}

bool ControlBase::_hasOverlay() const {
    return false;
}

void ControlBase::_drawOverlay(sf::RenderTarget&, sf::RenderStates) const {}

sf::Transform ControlBase::_getScreenTransform() const {
    sf::Transform transform = getTransform();
    transform.combine(presentationTransform());
    const std::shared_ptr<ControlBase> parent = getParent();
    if (parent != nullptr) {
        transform = parent->_getScreenTransform() * transform;
    }
    return transform;
}

void ControlBase::_applyRenderStates(sf::RenderStates& states) const {
    states.transform.translate(getPosition() * (Scale - 1.0f));
    states.transform.combine(getTransform());
    states.transform.combine(presentationTransform());
}

sf::Transform ControlBase::_getRenderTransform() const {
    sf::Transform transform;
    transform.translate(getPosition() * (Scale - 1.0f));
    transform.combine(getTransform());
    transform.combine(presentationTransform());
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

std::optional<sf::FloatRect>
ControlBase::_getAbsoluteChildInteractionClipBounds() const {
    return std::nullopt;
}

bool ControlBase::_ignoresAncestorInteractionClip() const {
    return false;
}

void ControlBase::_refreshPresentationColour() {}

const sf::Color& ControlBase::presentationColour() const {
    return presentationColour_;
}

sf::Color ControlBase::modulatePresentationColour(
    const sf::Color& colour) const {
    return {modulateColourComponent(colour.r, presentationColour_.r),
            modulateColourComponent(colour.g, presentationColour_.g),
            modulateColourComponent(colour.b, presentationColour_.b),
            modulateColourComponent(colour.a, presentationColour_.a)};
}

sf::Transform ControlBase::presentationTransform() const {
    sf::Transform result;
    result.translate(presentationTranslation_ * Scale);
    const sf::FloatRect bounds = getLocalBounds();
    const sf::Vector2f pivot =
        (bounds.position + sf::Vector2f(bounds.size.x * presentationPivot_.x,
                                        bounds.size.y * presentationPivot_.y)) *
        Scale;
    result.translate(pivot);
    result.rotate(sf::degrees(presentationRotation_));
    result.scale(presentationScale_);
    result.translate({-pivot.x, -pivot.y});
    return result;
}

void ControlBase::draw(sf::RenderTarget&, sf::RenderStates) const {}
