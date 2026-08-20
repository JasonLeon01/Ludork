#include <Gameplay/Actor.hpp>

#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool boundsIntersectCell(const sf::Vector2f& boundsPos,
                         const sf::Vector2f& boundsSize, float cellX,
                         float cellY, float cellSize) {
    const float boundsRight = boundsPos.x + boundsSize.x;
    const float boundsBottom = boundsPos.y + boundsSize.y;
    const float cellRight = cellX + cellSize;
    const float cellBottom = cellY + cellSize;
    return boundsPos.x < cellRight && boundsRight > cellX &&
           boundsPos.y < cellBottom && boundsBottom > cellY;
}

int roundHalfToEven(float value) {
    const float lower = std::floor(value);
    const float fraction = value - lower;
    if (fraction < 0.5f) {
        return static_cast<int>(lower);
    }
    if (fraction > 0.5f) {
        return static_cast<int>(lower + 1.0f);
    }
    const int lowerInteger = static_cast<int>(lower);
    return lowerInteger % 2 == 0 ? lowerInteger : lowerInteger + 1;
}

}  // namespace

ActorMapService::~ActorMapService() = default;

sf::Vector2f Actor::getPosition() const {
    return sf::Sprite::getPosition() - translation_;
}

sf::Vector2f Actor::getRelativePosition() const {
    return relativePosition_;
}

std::vector<sf::Vector2i> Actor::getOccupiedMapCells(
    const std::optional<sf::Vector2f>& worldPosition) const {
    syncMapCache();
    if (!worldPosition.has_value()) {
        return occupiedCells_;
    }
    const sf::Vector2f delta = *worldPosition - getPosition();
    const sf::Vector2i mapDelta(
        roundHalfToEven(delta.x / static_cast<float>(CellSize)),
        roundHalfToEven(delta.y / static_cast<float>(CellSize)));
    return getOccupiedMapCellsAtMapPosition(getMapPosition() + mapDelta);
}

std::vector<sf::Vector2i> Actor::getOccupiedMapCellsAtMapPosition(
    const sf::Vector2i& mapPosition) const {
    syncMapCache();
    const sf::Vector2i delta = mapPosition - cachedMapPosition_;
    const sf::Vector2f worldPosition(
        cachedPosition_.x + static_cast<float>(delta.x * CellSize),
        cachedPosition_.y + static_cast<float>(delta.y * CellSize));
    sf::FloatRect bounds = cachedGlobalBounds_;
    bounds.position += worldPosition - cachedPosition_;
    return computeOccupiedCells(bounds);
}

sf::Vector2i Actor::getRelativeMapPosition() const {
    return {
        static_cast<int>(relativePosition_.x / static_cast<float>(CellSize)),
        static_cast<int>(relativePosition_.y / static_cast<float>(CellSize))};
}

sf::FloatRect Actor::getLocalBounds() const {
    return sf::Sprite::getLocalBounds();
}

sf::FloatRect Actor::getGlobalBounds() const {
    return sf::Sprite::getGlobalBounds();
}

void Actor::setPosition(const sf::Vector2f& position) {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent != nullptr) {
        relativePosition_ = position - parent->getPosition();
    } else {
        relativePosition_ = {};
    }
    sf::Sprite::setPosition(position + translation_);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updatePositionFromParent();
    }
    syncMapCache();
}

void Actor::setRelativePosition(const sf::Vector2f& position) {
    const std::shared_ptr<Actor> parent = getParent();
    const sf::Vector2f parentPosition =
        parent == nullptr ? sf::Vector2f{} : parent->getPosition();
    setPosition(parentPosition + position);
}

void Actor::setMapPosition(const sf::Vector2u& position) {
    setPosition({static_cast<float>(position.x * CellSize),
                 static_cast<float>(position.y * CellSize)});
}

void Actor::setMapPosition(const sf::Vector2i& position) {
    if (position.x < 0 || position.y < 0) {
        throw std::invalid_argument("Map position must be non-negative");
    }
    setMapPosition(sf::Vector2u(static_cast<unsigned int>(position.x),
                                static_cast<unsigned int>(position.y)));
}

void Actor::setMapPositionSigned(const sf::Vector2i& position) {
    setMapPosition(position);
}

void Actor::setRelativeMapPosition(const sf::Vector2u& position) {
    setRelativePosition({static_cast<float>(position.x * CellSize),
                         static_cast<float>(position.y * CellSize)});
}

void Actor::move(const sf::Vector2f& offset) {
    sf::Sprite::move(offset);
    relativePosition_ += offset;
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updatePositionFromParent();
    }
    syncMapCache();
}

sf::Angle Actor::getRotation() const {
    return sf::Sprite::getRotation();
}

sf::Angle Actor::getRelativeRotation() const {
    return relativeRotation_;
}

void Actor::setRotation(sf::Angle angle) {
    const std::shared_ptr<Actor> parent = getParent();
    relativeRotation_ = parent == nullptr
                            ? sf::Angle::Zero
                            : sf::degrees(angle.asDegrees() -
                                          parent->getRotation().asDegrees());
    sf::Sprite::setRotation(angle);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateRotationFromParent();
    }
    syncMapCache();
}

void Actor::setRotationDegrees(float angle) {
    setRotation(sf::degrees(angle));
}

void Actor::rotate(sf::Angle angle) {
    relativeRotation_ =
        sf::degrees(relativeRotation_.asDegrees() + angle.asDegrees());
    sf::Sprite::rotate(angle);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateRotationFromParent();
    }
    syncMapCache();
}

void Actor::rotateDegrees(float angle) {
    rotate(sf::degrees(angle));
}

void Actor::setRelativeRotation(sf::Angle angle) {
    const std::shared_ptr<Actor> parent = getParent();
    const float parentDegrees =
        parent == nullptr ? 0.0f : parent->getRotation().asDegrees();
    setRotation(sf::degrees(parentDegrees + angle.asDegrees()));
}

void Actor::setRelativeRotationDegrees(float angle) {
    setRelativeRotation(sf::degrees(angle));
}

sf::Vector2f Actor::getScale() const {
    return sf::Sprite::getScale();
}

sf::Vector2f Actor::getRelativeScale() const {
    return relativeScale_;
}

void Actor::setScale(const sf::Vector2f& factors) {
    const std::shared_ptr<Actor> parent = getParent();
    relativeScale_ = parent == nullptr
                         ? sf::Vector2f{1.0f, 1.0f}
                         : factors.componentWiseDiv(parent->getScale());
    sf::Sprite::setScale(factors);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateScaleFromParent();
    }
    syncMapCache();
}

void Actor::scale(const sf::Vector2f& factor) {
    relativeScale_ = relativeScale_.componentWiseMul(factor);
    sf::Sprite::scale(factor);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateScaleFromParent();
    }
    syncMapCache();
}

void Actor::setRelativeScale(const sf::Vector2f& scale) {
    const std::shared_ptr<Actor> parent = getParent();
    const sf::Vector2f parentScale =
        parent == nullptr ? sf::Vector2f{1.0f, 1.0f} : parent->getScale();
    setScale(parentScale.componentWiseMul(scale));
}

sf::Vector2f Actor::getOrigin() const {
    return sf::Sprite::getOrigin();
}

void Actor::setOrigin(const sf::Vector2f& origin) {
    sf::Sprite::setOrigin(origin);
    syncMapCache();
}

sf::Vector2f Actor::getTranslation() const {
    return translation_;
}

void Actor::setTranslation(const sf::Vector2f& translation) {
    const sf::Vector2f position = getPosition();
    translation_ = translation;
    setPosition(position);
}

void Actor::setAlignment(const sf::Vector2f& alignment) {
    const sf::IntRect rect = sf::Sprite::getTextureRect();
    const float x = std::clamp(alignment.x, 0.0f, 1.0f);
    const float y = std::clamp(alignment.y, 0.0f, 1.0f);
    setOrigin({static_cast<float>(rect.size.x) * x,
               static_cast<float>(rect.size.y) * y});
}

std::shared_ptr<ActorMapService> Actor::getMap() const {
    return map_.lock();
}

void Actor::setMap(const std::shared_ptr<ActorMapService>& inMap) {
    map_ = ludork::engine::runtime_detail::canonicalRuntimeOwner(inMap);
}

std::shared_ptr<Actor> Actor::getParent() const {
    return parent_.lock();
}

void Actor::setParent(const std::shared_ptr<Actor>& parent) {
    parent_ = ludork::engine::runtime_detail::canonicalRuntimeOwner(parent);
}

const std::vector<std::shared_ptr<Actor>>& Actor::getChildren() const {
    return children_;
}

void Actor::addChild(const std::shared_ptr<Actor>& child) {
    if (!child || child.get() == this) {
        return;
    }
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator != children_.end()) {
        std::cerr << "Child already exists\n";
        return;
    }
    children_.push_back(child);
    const std::shared_ptr<Actor> self =
        std::static_pointer_cast<Actor>(weak_from_this().lock());
    if (self == nullptr) {
        throw std::logic_error("Actor hierarchy owner is not shared");
    }
    child->setParent(self);
    if (const std::shared_ptr<ActorMapService> map = getMap()) {
        child->setMap(map);
        map->updateActorList();
    }
}

void Actor::removeChild(const std::shared_ptr<Actor>& child) {
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator == children_.end()) {
        throw std::invalid_argument("Child not found");
    }
    children_.erase(iterator);
    child->setParent(nullptr);
}

void Actor::syncMapCache() const {
    Actor* self = const_cast<Actor*>(this);
    self->cachedPosition_ = getPosition();
    self->cachedGlobalBounds_ = sf::Sprite::getGlobalBounds();
    self->cachedMapPosition_ = sf::Vector2i(
        static_cast<int>(
            self->cachedPosition_.x / static_cast<float>(CellSize) + 0.5f),
        static_cast<int>(
            self->cachedPosition_.y / static_cast<float>(CellSize) + 0.5f));
    self->occupiedCells_ = computeOccupiedCells(self->cachedGlobalBounds_);
}

void Actor::_superMove(const sf::Vector2f& offset) {
    sf::Sprite::move(offset);
}

void Actor::_updatePositionFromParent() {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent == nullptr) {
        return;
    }
    sf::Sprite::setPosition(parent->getPosition() + relativePosition_);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updatePositionFromParent();
    }
}

void Actor::_updateRotationFromParent() {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent == nullptr) {
        return;
    }
    sf::Sprite::setRotation(sf::degrees(parent->getRotation().asDegrees() +
                                        relativeRotation_.asDegrees()));
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateRotationFromParent();
    }
}

void Actor::_updateScaleFromParent() {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent == nullptr) {
        return;
    }
    sf::Sprite::setScale(parent->getScale().componentWiseMul(relativeScale_));
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateScaleFromParent();
    }
}

bool Actor::intersects(const Actor& other) const {
    return getGlobalBounds()
        .findIntersection(other.getGlobalBounds())
        .has_value();
}

void Actor::refreshDescendantCache() {
    descendantActors_.clear();
    std::vector<Actor*> stack;
    stack.reserve(children_.size());
    for (const std::shared_ptr<Actor>& child : children_) {
        if (child != nullptr) {
            stack.push_back(child.get());
        }
    }
    while (!stack.empty()) {
        Actor* child = stack.back();
        stack.pop_back();
        if (!descendantActors_.insert(child).second) {
            continue;
        }
        for (const std::shared_ptr<Actor>& nextChild : child->children_) {
            if (nextChild != nullptr) {
                stack.push_back(nextChild.get());
            }
        }
    }
}

const std::unordered_set<Actor*>& Actor::getDescendantActors() const {
    return descendantActors_;
}

bool Actor::blocksPassability() const {
    return getCollisionEnabled() || getPathfindingBlocks();
}

std::vector<sf::Vector2i> Actor::computeOccupiedCells(
    const sf::FloatRect& bounds) const {
    if (bounds.size.x <= 0.f || bounds.size.y <= 0.f) {
        return {cachedMapPosition_};
    }
    const float cellSize = static_cast<float>(CellSize);
    const int minX = static_cast<int>(std::floor(bounds.position.x / cellSize));
    const int minY = static_cast<int>(std::floor(bounds.position.y / cellSize));
    const int maxX = static_cast<int>(
        std::floor((bounds.position.x + bounds.size.x - 1e-9f) / cellSize));
    const int maxY = static_cast<int>(
        std::floor((bounds.position.y + bounds.size.y - 1e-9f) / cellSize));
    std::vector<sf::Vector2i> cells;
    for (int cellY = minY; cellY <= maxY; ++cellY) {
        for (int cellX = minX; cellX <= maxX; ++cellX) {
            if (boundsIntersectCell(bounds.position, bounds.size,
                                    static_cast<float>(cellX) * cellSize,
                                    static_cast<float>(cellY) * cellSize,
                                    cellSize)) {
                cells.emplace_back(cellX, cellY);
            }
        }
    }
    if (cells.empty()) {
        return {cachedMapPosition_};
    }
    return cells;
}
