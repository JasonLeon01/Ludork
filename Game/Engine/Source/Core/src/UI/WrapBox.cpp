#include <UI/WrapBox.hpp>

#include <EngineState.hpp>
#include <UI/Canvas.hpp>
#include <UI/UiAssetInstance.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

void requireFinite(const sf::Vector2f& value, const char* name) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument(std::string("WrapBox ") + name +
                                    " must be finite");
    }
}

sf::FloatRect itemBounds(const ControlBase& child) {
    sf::Transform transform;
    transform.rotate(child.getRotation());
    transform.scale(child.getScale());
    transform.translate(-child.getOrigin());
    return transform.transformRect(
        {child.getLocalBounds().position, child.getSize()});
}

}  // namespace

WrapBox::WrapBox(const sf::Vector2f& size, int count,
                 const sf::Vector2f& spacing)
    : displayScale_(engineState().getScale()) {
    setSize(size);
    setSpacing(spacing);
    setCount(count);
}

WrapBox::~WrapBox() {
    clearItems();
}

sf::Vector2f WrapBox::getSize() const {
    return size_;
}

void WrapBox::setSize(const sf::Vector2f& size) {
    requireFinite(size, "size");
    if (size.x < 0.0f || size.y < 0.0f) {
        throw std::invalid_argument("WrapBox size cannot be negative");
    }
    size_ = size;
    applyPositions();
}

int WrapBox::getCount() const {
    return count_;
}

void WrapBox::setCount(int count) {
    if (count < 0) {
        throw std::invalid_argument("WrapBox count cannot be negative");
    }
    count_ = count;
    synchronizeCount();
    applyPositions();
}

sf::Vector2f WrapBox::getSpacing() const {
    return spacing_;
}

void WrapBox::setSpacing(const sf::Vector2f& spacing) {
    requireFinite(spacing, "spacing");
    for (const auto& instance : instances_) {
        const sf::Vector2f size = itemBounds(*instance->getRoot()).size;
        if ((spacing.x < 0.0f && size.x + spacing.x <= 0.0f) ||
            (spacing.y < 0.0f && size.y + spacing.y <= 0.0f)) {
            throw std::invalid_argument(
                "WrapBox spacing must leave a positive item step");
        }
    }
    spacing_ = spacing;
    applyPositions();
}

std::shared_ptr<ControlBase> WrapBox::get(int index) const {
    if (index < 1 || static_cast<std::size_t>(index) > instances_.size()) {
        throw std::out_of_range(
            "WrapBox index must be between 1 and its instance count");
    }
    return instances_[static_cast<std::size_t>(index - 1)]->getRoot();
}

std::vector<std::shared_ptr<ControlBase>> WrapBox::getChildren() const {
    std::vector<std::shared_ptr<ControlBase>> result;
    result.reserve(instances_.size());
    for (const auto& instance : instances_) {
        result.push_back(instance->getRoot());
    }
    return result;
}

sf::FloatRect WrapBox::getContentBounds() const {
    const_cast<WrapBox*>(this)->applyPositions();
    sf::FloatRect result{{0.0f, 0.0f}, size_};
    for (const auto& instance : instances_) {
        const auto child = instance->getRoot();
        if (!child->getVisible()) {
            continue;
        }
        sf::FloatRect bounds = itemBounds(*child);
        bounds.position += child->getPosition();
        const sf::Vector2f start{
            std::min(result.position.x, bounds.position.x),
            std::min(result.position.y, bounds.position.y)};
        const sf::Vector2f end{std::max(result.position.x + result.size.x,
                                        bounds.position.x + bounds.size.x),
                               std::max(result.position.y + result.size.y,
                                        bounds.position.y + bounds.size.y)};
        result = {start, end - start};
    }
    return result;
}

sf::Vector2f WrapBox::getOrigin() const {
    return ControlBase::getOrigin() / displayScale_;
}
void WrapBox::setOrigin(const sf::Vector2f& origin) {
    ControlBase::setOrigin(origin * displayScale_);
}
sf::RenderStates WrapBox::getRenderStates() const {
    return canvasRenderStates();
}

void WrapBox::setTemplateFactory(
    std::function<std::shared_ptr<UiAssetInstance>()> factory) {
    clearItems();
    factory_ = std::move(factory);
    synchronizeCount();
    applyPositions();
}

const std::vector<std::shared_ptr<UiAssetInstance>>& WrapBox::getInstances()
    const {
    return instances_;
}

void WrapBox::synchronizeCount() {
    while (instances_.size() > static_cast<std::size_t>(count_)) {
        const auto root = instances_.back()->getRoot();
        root->releaseRuntimeCallbacks();
        root->setParent(nullptr);
        instances_.pop_back();
    }
    if (!factory_) {
        return;
    }
    const auto owner = weak_from_this().lock();
    if (!owner) {
        throw std::logic_error("WrapBox owner is not shared");
    }
    while (instances_.size() < static_cast<std::size_t>(count_)) {
        auto instance = factory_();
        if (!instance) {
            throw std::invalid_argument(
                "WrapBox template returned no instance");
        }
        instance->getRoot()->setParent(owner);
        instances_.push_back(std::move(instance));
    }
}

void WrapBox::clearItems() {
    for (const auto& instance : instances_) {
        instance->getRoot()->releaseRuntimeCallbacks();
        instance->getRoot()->setParent(nullptr);
    }
    instances_.clear();
}

void WrapBox::reflowItems() {
    for (const auto& instance : instances_) {
        instance->reflow();
    }
    applyPositions();
}

void WrapBox::applyPositions() {
    float x = 0.0f;
    float y = 0.0f;
    float rowHeight = 0.0f;
    bool first = true;
    for (const auto& instance : instances_) {
        const auto root = instance->getRoot();
        const sf::FloatRect bounds = itemBounds(*root);
        if ((spacing_.x < 0.0f && bounds.size.x + spacing_.x <= 0.0f) ||
            (spacing_.y < 0.0f && bounds.size.y + spacing_.y <= 0.0f)) {
            throw std::invalid_argument(
                "WrapBox spacing must leave a positive item step");
        }
        if (!first && x + bounds.size.x > size_.x) {
            x = 0.0f;
            y += rowHeight + spacing_.y;
            rowHeight = 0.0f;
        }
        root->setPosition(sf::Vector2f{x, y} - bounds.position);
        x += bounds.size.x + spacing_.x;
        rowHeight = std::max(rowHeight, bounds.size.y);
        first = false;
    }
}

void WrapBox::update(float deltaTime) {
    applyPositions();
    for (const auto& child : getChildren()) {
        if (auto functional = ludork::Cast<FunctionalBase>(child.get());
            functional && child->getVisible()) {
            functional->update(deltaTime);
        }
    }
    FunctionalBase::update(deltaTime);
}

void WrapBox::lateUpdate(float deltaTime) {
    for (const auto& child : getChildren()) {
        if (auto functional = ludork::Cast<FunctionalBase>(child.get());
            functional && child->getVisible()) {
            functional->lateUpdate(deltaTime);
        }
    }
    FunctionalBase::lateUpdate(deltaTime);
}

void WrapBox::fixedUpdate(float fixedDelta) {
    for (const auto& child : getChildren()) {
        if (auto functional = ludork::Cast<FunctionalBase>(child.get());
            functional && child->getVisible()) {
            functional->fixedUpdate(fixedDelta);
        }
    }
    FunctionalBase::fixedUpdate(fixedDelta);
}

void WrapBox::refreshDisplayScale() {
    const sf::Vector2f origin = getOrigin();
    displayScale_ = engineState().getScale();
    setOrigin(origin);
    ControlBase::refreshDisplayScale();
    applyPositions();
}

void WrapBox::releaseRuntimeCallbacks() noexcept {
    ControlBase::releaseRuntimeCallbacks();
    factory_ = {};
}

void WrapBox::dispose() {
    releaseRuntimeCallbacks();
    clearItems();
}

void WrapBox::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (!getVisible()) {
        return;
    }
    const_cast<WrapBox*>(this)->applyPositions();
    _applyRenderStates(states);
    for (const auto& child : getChildren()) {
        if (!child->getVisible()) {
            continue;
        }
        if (auto canvas = ludork::Cast<Canvas>(child.get())) {
            canvas->render();
        }
        target.draw(*child, states);
    }
}
