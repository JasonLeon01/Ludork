#include <Gameplay/ActorCore.hpp>

#include <algorithm>
#include <cmath>

namespace {
bool boundsIntersectCell(const sf::Vector2f &boundsPos, const sf::Vector2f &boundsSize, float cellX, float cellY,
                         float cellSize) {
    const float boundsRight = boundsPos.x + boundsSize.x;
    const float boundsBottom = boundsPos.y + boundsSize.y;
    const float cellRight = cellX + cellSize;
    const float cellBottom = cellY + cellSize;
    return boundsPos.x < cellRight && boundsRight > cellX && boundsPos.y < cellBottom && boundsBottom > cellY;
}
} // namespace

ActorCore::ActorCore(int cellSize)
    : cellSize_(cellSize > 0 ? cellSize : 32),
      position_(0.f, 0.f),
      mapPosition_(0, 0),
      globalBounds_({0.f, 0.f}, {0.f, 0.f}),
      pathfindingBlocks_(false) {}

void ActorCore::syncFromGlobalBounds(const sf::Vector2f &position, const sf::FloatRect &globalBounds) {
    position_ = position;
    globalBounds_ = globalBounds;
    mapPosition_ = sf::Vector2i(static_cast<int>(position.x / static_cast<float>(cellSize_) + 0.5f),
                                static_cast<int>(position.y / static_cast<float>(cellSize_) + 0.5f));
    occupiedCells_ = computeOccupiedCells(globalBounds_);
}

void ActorCore::syncFromSprite(const sf::Vector2f &position, const sf::Vector2f &scale, const sf::Vector2f &origin,
                               const sf::Vector2f &localBoundsSize) {
    position_ = position;
    sf::Transform transform = sf::Transform::Identity;
    transform.translate(position);
    transform.scale(scale);
    transform.translate(-origin);
    globalBounds_ = transform.transformRect(sf::FloatRect({0.f, 0.f}, localBoundsSize));
    mapPosition_ = sf::Vector2i(static_cast<int>(position.x / static_cast<float>(cellSize_) + 0.5f),
                                static_cast<int>(position.y / static_cast<float>(cellSize_) + 0.5f));
    occupiedCells_ = computeOccupiedCells(globalBounds_);
}

sf::Vector2i ActorCore::getMapPosition() const { return mapPosition_; }

sf::FloatRect ActorCore::getGlobalBounds() const { return globalBounds_; }

std::vector<sf::Vector2i> ActorCore::getOccupiedMapCells() const { return occupiedCells_; }

std::vector<sf::Vector2i> ActorCore::getOccupiedMapCellsAtMapPosition(const sf::Vector2i &mapPosition) const {
    const sf::Vector2i delta = mapPosition - mapPosition_;
    const sf::Vector2f worldPosition(position_.x + static_cast<float>(delta.x) * static_cast<float>(cellSize_),
                                     position_.y + static_cast<float>(delta.y) * static_cast<float>(cellSize_));
    sf::FloatRect bounds = globalBounds_;
    bounds.position += worldPosition - position_;
    return computeOccupiedCells(bounds);
}

bool ActorCore::getPathfindingBlocks() const { return pathfindingBlocks_; }

void ActorCore::setPathfindingBlocks(bool blocks) { pathfindingBlocks_ = blocks; }

void ActorCore::addChild(ActorCore *child) {
    if (child == nullptr || child == this) {
        return;
    }
    for (ActorCore *existing : children_) {
        if (existing == child) {
            return;
        }
    }
    child->parent_ = this;
    children_.push_back(child);
}

void ActorCore::removeChild(ActorCore *child) {
    if (child == nullptr) {
        throw py::value_error("Child not found");
    }
    const auto it = std::find(children_.begin(), children_.end(), child);
    if (it == children_.end()) {
        throw py::value_error("Child not found");
    }
    children_.erase(it);
}

void ActorCore::setParent(ActorCore *parent) { parent_ = parent; }

void ActorCore::refreshDescendantIds() {
    descendantIds_.clear();
    std::vector<ActorCore *> stack;
    stack.reserve(children_.size());
    for (ActorCore *child : children_) {
        stack.push_back(child);
    }
    while (!stack.empty()) {
        ActorCore *child = stack.back();
        stack.pop_back();
        if (!descendantIds_.insert(child).second) {
            continue;
        }
        for (ActorCore *nextChild : child->children_) {
            stack.push_back(nextChild);
        }
    }
}

py::object ActorCore::getPythonActor() const { return py::cast(const_cast<ActorCore *>(this)); }

const std::unordered_set<ActorCore *> &ActorCore::getDescendantIds() const { return descendantIds_; }

bool ActorCore::blocksPassability() const { return collisionEnabled || pathfindingBlocks_; }

std::vector<sf::Vector2i> ActorCore::computeOccupiedCells(const sf::FloatRect &bounds) const {
    if (bounds.size.x <= 0.f || bounds.size.y <= 0.f) {
        return {mapPosition_};
    }
    const float cellSize = static_cast<float>(cellSize_);
    const int minX = static_cast<int>(std::floor(bounds.position.x / cellSize));
    const int minY = static_cast<int>(std::floor(bounds.position.y / cellSize));
    const int maxX = static_cast<int>(std::floor((bounds.position.x + bounds.size.x - 1e-9f) / cellSize));
    const int maxY = static_cast<int>(std::floor((bounds.position.y + bounds.size.y - 1e-9f) / cellSize));
    std::vector<sf::Vector2i> cells;
    for (int cellY = minY; cellY <= maxY; ++cellY) {
        for (int cellX = minX; cellX <= maxX; ++cellX) {
            if (boundsIntersectCell(bounds.position, bounds.size, static_cast<float>(cellX) * cellSize,
                                    static_cast<float>(cellY) * cellSize, cellSize)) {
                cells.emplace_back(cellX, cellY);
            }
        }
    }
    if (cells.empty()) {
        return {mapPosition_};
    }
    return cells;
}
