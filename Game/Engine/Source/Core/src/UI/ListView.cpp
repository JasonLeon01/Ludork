#include "Containers/ContainerSupport.hpp"
#include "Containers/ChildClipGuard.hpp"
#include <UI/ListView.hpp>

#include <EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

ListView::ListView(const sf::IntRect& rect, int defaultItemHeight,
                   bool fixItemHeight, int columns)
    : size_(static_cast<float>(rect.size.x), static_cast<float>(rect.size.y)),
      defaultItemHeight_(defaultItemHeight),
      fixItemHeight_(fixItemHeight),
      columns_(columns),
      renderStates_(canvasRenderStates()),
      displayScale_(engineState().getScale()) {
    if (columns_ <= 0) {
        throw std::invalid_argument("ListView columns must be positive");
    }
    setPosition({static_cast<float>(rect.position.x),
                 static_cast<float>(rect.position.y)});
}

sf::Vector2f ListView::getOrigin() const {
    return ControlBase::getOrigin() / engineState().getScale();
}

void ListView::setOrigin(const sf::Vector2f& origin) {
    ControlBase::setOrigin(origin * engineState().getScale());
}

int ListView::getColumns() const {
    return columns_;
}

sf::Vector2f ListView::getDefaultItemSize() const {
    return {(size_.x - 32.0f) / columns_,
            static_cast<float>(defaultItemHeight_)};
}

sf::FloatRect ListView::getItemLayoutRect(int index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= children_.size()) {
        throw std::out_of_range("ListView item index out of range");
    }
    const_cast<ListView*>(this)->applyPositions();
    return itemLayoutRects_[static_cast<std::size_t>(index)];
}

sf::Vector2f ListView::getSize() const {
    return size_;
}

sf::FloatRect ListView::getContentBounds() const {
    const_cast<ListView*>(this)->applyPositions();
    sf::FloatRect bounds = ludork::engine::ui_container::aggregateContentBounds(
        {{0.0f, 0.0f}, size_}, children_);
    if (childScrollingEnabled_) {
        bounds.position.x = 0.0f;
        bounds.size.x = size_.x;
    }
    return bounds;
}

void ListView::setSize(const sf::Vector2i& size) {
    setSizeValue({static_cast<float>(size.x), static_cast<float>(size.y)});
}

void ListView::setSizeVector2u(const sf::Vector2u& size) {
    setSizeValue({static_cast<float>(size.x), static_cast<float>(size.y)});
}

void ListView::setSizeVector2f(const sf::Vector2f& size) {
    setSizeValue(size);
}

void ListView::setColumns(int columns) {
    if (columns <= 0) {
        throw std::invalid_argument("ListView columns must be positive");
    }
    columns_ = columns;
    positionsSettled_ = false;
}

bool ListView::getChildScrollingEnabled() const {
    return childScrollingEnabled_;
}

void ListView::setChildScrollingEnabled(bool enabled) {
    if (childScrollingEnabled_ == enabled) {
        return;
    }
    childScrollingEnabled_ = enabled;
    for (ChildScrollState& state : childScrollStates_) {
        state.elapsed = 0.0;
    }
    invalidatePositions();
    applyPositions();
}

std::vector<std::shared_ptr<ControlBase>> ListView::getChildren() const {
    return children_;
}

void ListView::addChild(const std::shared_ptr<ControlBase>& child) {
    if (child == nullptr) {
        throw std::invalid_argument("ListView child cannot be null");
    }
    const std::shared_ptr<ControlBase> self = weak_from_this().lock();
    if (self == nullptr) {
        throw std::logic_error("ListView owner is not shared");
    }
    children_.push_back(child);
    childScrollStates_.emplace_back();
    child->setParent(self);
    positionsSettled_ = false;
}

void ListView::removeChild(const std::shared_ptr<ControlBase>& child) {
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator == children_.end()) {
        throw std::invalid_argument("ListView child not found");
    }
    (*iterator)->setParent(nullptr);
    childScrollStates_.erase(childScrollStates_.begin() +
                             std::distance(children_.begin(), iterator));
    children_.erase(iterator);
    positionsSettled_ = false;
}

void ListView::clearChildren() {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        child->setParent(nullptr);
    }
    children_.clear();
    childScrollStates_.clear();
    positionsSettled_ = false;
}

sf::RenderStates ListView::getRenderStates() const {
    return renderStates_;
}

void ListView::update(float deltaTime) {
    ludork::engine::ui_container::tickChildren(
        children_, &FunctionalBase::update, deltaTime);
    FunctionalBase::update(deltaTime);
    updateChildScrolling(deltaTime);
}

void ListView::updateChildScrolling(float deltaTime) {
    if (!childScrollingEnabled_ || !getVisible()) {
        return;
    }
    for (std::shared_ptr<ControlBase> parent = getParent(); parent != nullptr;
         parent = parent->getParent()) {
        if (!parent->getVisible()) {
            return;
        }
    }
    invalidatePositions();
    applyPositions();
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        return;
    }
    for (std::size_t index = 0; index < children_.size(); ++index) {
        ChildScrollState& state = childScrollStates_[index];
        if (children_[index]->getVisible() && state.distance > 0.0f) {
            const double cycle =
                2.0 * (ChildScrollPauseDuration +
                       static_cast<double>(state.distance) / ChildScrollSpeed);
            state.elapsed = std::fmod(state.elapsed + deltaTime, cycle);
        }
    }
    invalidatePositions();
    applyPositions();
}

float ListView::childScrollOffset(const ChildScrollState& state) {
    const double travel =
        static_cast<double>(state.distance) / ChildScrollSpeed;
    const double forward =
        std::clamp(state.elapsed - ChildScrollPauseDuration, 0.0, travel);
    const double backward = std::clamp(
        state.elapsed - 2.0 * ChildScrollPauseDuration - travel, 0.0, travel);
    return static_cast<float>((forward - backward) * ChildScrollSpeed);
}

void ListView::lateUpdate(float deltaTime) {
    ludork::engine::ui_container::tickChildren(
        children_, &FunctionalBase::lateUpdate, deltaTime);
    FunctionalBase::lateUpdate(deltaTime);
}

void ListView::fixedUpdate(float fixedDelta) {
    ludork::engine::ui_container::tickChildren(
        children_, &FunctionalBase::fixedUpdate, fixedDelta);
    FunctionalBase::fixedUpdate(fixedDelta);
}

void ListView::invalidatePositions() {
    positionsSettled_ = false;
}

void ListView::applyPositions() {
    if (positionsSettled_) {
        return;
    }
    positionsSettled_ = true;
    const sf::Vector2f defaultSize = getDefaultItemSize();
    const float columnWidth = defaultSize.x;
    const float inset = (size_.x - columnWidth * columns_) / 2.0f;
    itemLayoutRects_.resize(children_.size());
    float currentY = 0.0f;
    for (std::size_t rowStart = 0; rowStart < children_.size();
         rowStart += static_cast<std::size_t>(columns_)) {
        const std::size_t rowEnd = std::min(
            rowStart + static_cast<std::size_t>(columns_), children_.size());
        float rowHeight = defaultSize.y;
        if (!fixItemHeight_) {
            for (std::size_t index = rowStart; index < rowEnd; ++index) {
                rowHeight = std::max(rowHeight, children_[index]->getSize().y);
            }
        }
        for (std::size_t index = rowStart; index < rowEnd; ++index) {
            const std::shared_ptr<ControlBase>& child = children_[index];
            const float columnX =
                inset + static_cast<float>(index - rowStart) * columnWidth;
            itemLayoutRects_[index] = {{columnX, currentY},
                                       {columnWidth, rowHeight}};
            const sf::FloatRect bounds = child->getLocalBounds();
            float positionX = columnX + columnWidth / 2.0f -
                              (bounds.position.x + bounds.size.x / 2.0f -
                               child->getOrigin().x) *
                                  child->getScale().x;
            if (childScrollingEnabled_) {
                ChildScrollState& state = childScrollStates_[index];
                const sf::Vector2f origin = child->getOrigin();
                const sf::Vector2f scale = child->getScale();
                if (state.bounds != bounds || state.origin != origin ||
                    state.scale != scale || state.columnWidth != columnWidth) {
                    state.bounds = bounds;
                    state.origin = origin;
                    state.scale = scale;
                    state.columnWidth = columnWidth;
                    state.elapsed = 0.0;
                }
                state.distance =
                    std::max(0.0f, std::abs(bounds.size.x * scale.x) -
                                       std::max(0.0f, columnWidth));
                if (state.distance > 0.0f) {
                    const float left = std::min(
                        (bounds.position.x - origin.x) * scale.x,
                        (bounds.position.x + bounds.size.x - origin.x) *
                            scale.x);
                    positionX = columnX - left - childScrollOffset(state);
                }
            }
            child->setPosition({positionX, currentY});
        }
        currentY += rowHeight;
    }
}

sf::FloatRect ListView::clipChildBounds(
    const ControlBase& child, const sf::FloatRect& absoluteBounds) const {
    if (!childScrollingEnabled_) {
        return absoluteBounds;
    }
    const auto iterator =
        std::find_if(children_.begin(), children_.end(),
                     [&child](const std::shared_ptr<ControlBase>& item) {
                         return item.get() == &child;
                     });
    if (iterator == children_.end()) {
        return absoluteBounds;
    }
    const sf::FloatRect cell = getItemLayoutRect(
        static_cast<int>(std::distance(children_.begin(), iterator)));
    sf::Transform transform = screenRenderTransform();
    const float scale = engineState().getScale();
    transform.scale({scale, scale});
    sf::FloatRect local = transform.getInverse().transformRect(absoluteBounds);
    const float left = std::max(local.position.x, cell.position.x);
    const float right = std::min(local.position.x + local.size.x,
                                 cell.position.x + std::max(0.0f, cell.size.x));
    if (right <= left) {
        return {{0.0f, 0.0f}, {0.0f, 0.0f}};
    }
    local.position.x = left;
    local.size.x = right - left;
    return transform.transformRect(local)
        .findIntersection(absoluteBounds)
        .value_or(sf::FloatRect{});
}

sf::FloatRect ListView::_clipChildInteractionBounds(
    const ControlBase& child, const sf::FloatRect& bounds) const {
    return clipChildBounds(child, bounds);
}

void ListView::refreshDisplayScale() {
    if (displayScale_ != engineState().getScale()) {
        const sf::Vector2f logicalOrigin =
            ControlBase::getOrigin() / displayScale_;
        displayScale_ = engineState().getScale();
        setOrigin(logicalOrigin);
        invalidatePositions();
    }
    ControlBase::refreshDisplayScale();
}

void ListView::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (!getVisible()) {
        return;
    }
    const_cast<ListView*>(this)->applyPositions();
    states.transform.combine(renderTransform());
    for (const std::shared_ptr<ControlBase>& child : children_) {
        if (child->getVisible()) {
            ludork::engine::ui_container::ChildClipGuard clip(target, *child,
                                                              states.transform);
            if (!clip.empty()) {
                target.draw(*child, states);
            }
        }
    }
}

void ListView::setSizeValue(const sf::Vector2f& size) {
    size_ = size;
    positionsSettled_ = false;
}
