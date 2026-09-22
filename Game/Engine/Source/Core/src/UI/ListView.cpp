#include <UI/ListView.hpp>

#include <EngineState.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
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
    sf::FloatRect bounds{{0.0f, 0.0f}, size_};
    for (const std::shared_ptr<ControlBase>& child : children_) {
        if (child == nullptr || !child->getVisible()) {
            continue;
        }
        const sf::FloatRect childBounds =
            child->getTransform().transformRect(child->getContentBounds());
        const float minimumX =
            std::min(bounds.position.x, childBounds.position.x);
        const float minimumY =
            std::min(bounds.position.y, childBounds.position.y);
        const float maximumX =
            std::max(bounds.position.x + bounds.size.x,
                     childBounds.position.x + childBounds.size.x);
        const float maximumY =
            std::max(bounds.position.y + bounds.size.y,
                     childBounds.position.y + childBounds.size.y);
        bounds = {{minimumX, minimumY},
                  {maximumX - minimumX, maximumY - minimumY}};
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
    child->setParent(self);
    positionsSettled_ = false;
}

void ListView::removeChild(const std::shared_ptr<ControlBase>& child) {
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator == children_.end()) {
        throw std::invalid_argument("ListView child not found");
    }
    (*iterator)->setParent(nullptr);
    children_.erase(iterator);
    positionsSettled_ = false;
}

void ListView::clearChildren() {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        child->setParent(nullptr);
    }
    children_.clear();
    positionsSettled_ = false;
}

sf::RenderStates ListView::getRenderStates() const {
    return renderStates_;
}

void ListView::update(float deltaTime) {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        FunctionalBase* functional = ludork::Cast<FunctionalBase>(child.get());
        if (functional != nullptr && child->getVisible()) {
            functional->update(deltaTime);
        }
    }
    FunctionalBase::update(deltaTime);
}

void ListView::lateUpdate(float deltaTime) {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        FunctionalBase* functional = ludork::Cast<FunctionalBase>(child.get());
        if (functional != nullptr && child->getVisible()) {
            functional->lateUpdate(deltaTime);
        }
    }
}

void ListView::fixedUpdate(float fixedDelta) {
    for (const std::shared_ptr<ControlBase>& child : children_) {
        FunctionalBase* functional = ludork::Cast<FunctionalBase>(child.get());
        if (functional != nullptr && child->getVisible()) {
            functional->fixedUpdate(fixedDelta);
        }
    }
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
            const float positionX = columnX + columnWidth / 2.0f -
                                    (bounds.position.x + bounds.size.x / 2.0f -
                                     child->getOrigin().x) *
                                        child->getScale().x;
            child->setPosition({positionX, currentY});
        }
        currentY += rowHeight;
    }
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
    const_cast<ListView*>(this)->applyPositions();
    states.transform.combine(getTransform());
    for (const std::shared_ptr<ControlBase>& child : children_) {
        target.draw(*child, states);
    }
}

void ListView::setSizeValue(const sf::Vector2f& size) {
    size_ = size;
    positionsSettled_ = false;
}
