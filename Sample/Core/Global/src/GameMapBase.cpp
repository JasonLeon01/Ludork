#include "GameMapBase.hpp"

#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>
#include <Runtime/EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>
#include <variant>

using Node = std::pair<int, IntPair>;

inline bool inBounds(int x, int y, int width, int height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}

std::size_t IntPairHash::operator()(const IntPair& value) const {
    std::size_t xHash = std::hash<int>{}(value.first);
    std::size_t yHash = std::hash<int>{}(value.second);
    return xHash ^ (yHash + 0x9e3779b9 + (xHash << 6) + (xHash >> 2));
}

inline int getNodeFScore(const std::map<IntPair, int>& fScore,
                         const IntPair& node) {
    auto iter = fScore.find(node);
    if (iter == fScore.end()) {
        return 1 << 30;
    }
    return iter->second;
}

static float materialValueToFloat(const MaterialValue& value) {
    return std::visit(
        [](const auto& item) -> float {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, bool>) {
                return item ? 1.0f : 0.0f;
            } else {
                return item;
            }
        },
        value);
}

void GameMapBase::setTilemap(std::shared_ptr<Tilemap> tilemap) {
    tilemap_ = std::move(tilemap);
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

std::shared_ptr<sf::Texture> GameMapBase::generateDataFromMap(
    const sf::Vector2u& size,
    const std::vector<std::vector<MaterialValue>>& materialMap, bool smooth) {
    int dataLen = size.x * size.y * 4;
    std::vector<std::uint8_t> pixelData(dataLen);
    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            int index = (y * size.x + x) * 4;
            pixelData[index] =
                std::uint8_t(materialValueToFloat(materialMap[y][x]) * 255.0f);
            pixelData[index + 1] = pixelData[index];
            pixelData[index + 2] = pixelData[index];
            pixelData[index + 3] = 255;
        }
    }

    sf::Image img(size, pixelData.data());
    std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromImage(img)) {
        throw std::runtime_error(
            "Failed to load texture from image at method generateDataFromMap");
    }
    texture->setSmooth(smooth);
    return texture;
}

PathResult GameMapBase::findPathExt(
    const sf::Vector2i& start, const sf::Vector2i& goal,
    const sf::Vector2u& size, Actor& movingActor,
    const std::vector<sf::Vector2i>& excludedAnchors) {
    ensurePassabilityCache();
    refreshActorOccupancyCache();
    int sx = start.x;
    int sy = start.y;
    int gx = goal.x;
    int gy = goal.y;
    unsigned int width = size.x;
    unsigned int height = size.y;
    IntPair dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    std::unordered_set<IntPair, IntPairHash> excludedAnchorSet;
    excludedAnchorSet.reserve(excludedAnchors.size());
    for (const sf::Vector2i& anchor : excludedAnchors) {
        excludedAnchorSet.emplace(anchor.x, anchor.y);
    }
    PathResult result;
    if (start_t == goal_t) {
        result.route.emplace_back(sx, sy);
        return result;
    }
    std::map<IntPair, IntPair> cameFrom;
    std::map<IntPair, int> gScore;
    gScore[start_t] = 0;
    std::map<IntPair, int> fScore;
    fScore[start_t] = std::abs(sx - gx) + std::abs(sy - gy);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openQueue;
    openQueue.push({fScore[start_t], start_t});
    while (!openQueue.empty()) {
        IntPair current = openQueue.top().second;
        int currentF = openQueue.top().first;
        openQueue.pop();
        if (currentF > getNodeFScore(fScore, current)) {
            continue;
        }
        if (current == goal_t) {
            std::vector<IntPair> pathPosition;
            auto c = current;
            while (cameFrom.find(c) != cameFrom.end()) {
                pathPosition.push_back(c);
                c = cameFrom[c];
            }
            std::reverse(pathPosition.begin(), pathPosition.end());
            result.offsets.reserve(pathPosition.size());
            result.points.reserve(pathPosition.size());
            result.route.reserve(pathPosition.size() + 1);
            result.route.emplace_back(sx, sy);
            int px = sx;
            int py = sy;
            for (auto& [x, y] : pathPosition) {
                result.offsets.emplace_back(x - px, y - py);
                result.points.emplace_back(x, y);
                result.route.emplace_back(x, y);
                px = x;
                py = y;
            }
            return result;
        }
        auto [cx, cy] = current;
        for (auto& [dx, dy] : dirs) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (!inBounds(nx, ny, width, height)) {
                continue;
            }
            const IntPair nt = {nx, ny};
            if (nt != start_t &&
                excludedAnchorSet.find(nt) != excludedAnchorSet.end()) {
                continue;
            }
            if (!nodePassableForActor(nx, ny, sx, sy, gx, gy, width, height,
                                      movingActor)) {
                continue;
            }
            int tentative = gScore[current] + 1;
            int prevG = (gScore.count(nt)) ? gScore[nt] : (1 << 30);
            if (tentative < prevG) {
                cameFrom[nt] = current;
                gScore[nt] = tentative;
                int nextF = tentative + std::abs(nx - gx) + std::abs(ny - gy);
                fScore[nt] = nextF;
                openQueue.push({nextF, nt});
            }
        }
    }
    return result;
}

std::vector<std::vector<MaterialValue>> GameMapBase::getMaterialPropertyMapExt(
    int width, int height, const std::string& propertyName,
    const MaterialValue& invalidValue) {
    std::vector<std::vector<MaterialValue>> materialPropertyMap;
    materialPropertyMap.reserve(static_cast<std::size_t>(std::max(height, 0)));
    for (int y = 0; y < height; ++y) {
        materialPropertyMap.push_back({});
        materialPropertyMap.back().reserve(
            static_cast<std::size_t>(std::max(width, 0)));
        for (int x = 0; x < width; ++x) {
            materialPropertyMap[y].push_back(getMaterialProperty(
                {x, height - y - 1}, propertyName, invalidValue));
        }
    }
    return materialPropertyMap;
}

void GameMapBase::syncActorsRef(const ActorDict& actors) {
    occupancyMap_.clear();
    registeredOccupancyCells_.clear();
    actorLayerRef_.clear();
    actorsRef_.clear();
    for (const auto& [layerName, actorList] : actors) {
        std::vector<ActorPtr>& actorRefs = actorsRef_[layerName];
        actorRefs.reserve(actorList.size());
        for (const ActorPtr& actor : actorList) {
            if (!actor) {
                continue;
            }
            actorRefs.push_back(actor);
            actorLayerRef_[actor.get()] = layerName;
        }
    }
    passabilityDirty_ = true;
}

void GameMapBase::syncMaterialActorsRef(const ActorDict& actors) {
    materialActorsRef_.clear();
    for (const auto& [layerName, actorList] : actors) {
        std::vector<ActorPtr>& actorRefs = materialActorsRef_[layerName];
        actorRefs.reserve(actorList.size());
        for (const ActorPtr& actor : actorList) {
            if (actor) {
                actorRefs.push_back(actor);
            }
        }
    }
}

std::vector<std::vector<bool>> GameMapBase::rebuildPassabilityCache(
    const sf::Vector2u& size) {
    unsigned int width = size.x;
    unsigned int height = size.y;
    const std::vector<std::string> layerNames = getTopFirstLayerNames();
    std::vector<std::vector<bool>> tilePassableGrid(height);
    occupancyMap_.clear();
    registeredOccupancyCells_.clear();
    for (unsigned int y = 0; y < height; ++y) {
        std::vector<bool> row(width);
        for (unsigned int x = 0; x < width; ++x) {
            bool passable = true;
            const sf::Vector2i position(static_cast<int>(x),
                                        static_cast<int>(y));
            for (const std::string& layerName : layerNames) {
                const std::shared_ptr<TileLayer> layer =
                    tilemap_->getLayer(layerName);
                if (!layer || !layer->getVisible() ||
                    !layer->hasContent(position)) {
                    continue;
                }
                passable = layer->isPassable(position);
                break;
            }
            row[x] = passable;
        }
        tilePassableGrid[y] = row;
    }
    for (auto& [_, actorList] : actorsRef_) {
        for (const ActorPtr& actor : actorList) {
            registerActorOccupancy(*actor);
        }
    }
    tilePassableGrid_ = tilePassableGrid;
    passabilityDirty_ = false;
    return tilePassableGrid_;
}

void GameMapBase::invalidatePassabilityCache() {
    passabilityDirty_ = true;
}

sf::Vector2u GameMapBase::getSize() const {
    return tilemap_ == nullptr ? sf::Vector2u{} : tilemap_->getSize();
}

bool GameMapBase::isPassable(const Actor& actor,
                             const sf::Vector2i& position) const {
    if (!actor.getCollisionEnabled()) {
        return true;
    }
    ensurePassabilityCache();
    const sf::Vector2u size = getSize();
    const std::vector<sf::Vector2i> occupied =
        actor.getOccupiedMapCellsAtMapPosition(position);
    GameMapBase* self = const_cast<GameMapBase*>(this);
    for (const sf::Vector2i& cell : occupied) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= static_cast<int>(size.x) ||
            cell.y >= static_cast<int>(size.y)) {
            return false;
        }
        if (!tilePassableGrid_[static_cast<std::size_t>(cell.y)]
                              [static_cast<std::size_t>(cell.x)]) {
            return false;
        }
    }
    const sf::Vector2i currentPosition = actor.getMapPosition();
    const sf::Vector2i delta = position - currentPosition;
    int direction = -1;
    if (delta == sf::Vector2i(0, 1)) {
        direction = Direction.at("DOWN");
    } else if (delta == sf::Vector2i(0, -1)) {
        direction = Direction.at("UP");
    } else if (delta == sf::Vector2i(1, 0)) {
        direction = Direction.at("RIGHT");
    } else if (delta == sf::Vector2i(-1, 0)) {
        direction = Direction.at("LEFT");
    }
    if (direction >= 0) {
        const std::vector<sf::Vector2i> currentCells =
            actor.getOccupiedMapCellsAtMapPosition(currentPosition);
        std::unordered_set<IntPair, IntPairHash> currentCellSet;
        for (const sf::Vector2i& cell : currentCells) {
            currentCellSet.emplace(cell.x, cell.y);
        }
        for (const sf::Vector2i& cell : occupied) {
            if (currentCellSet.contains({cell.x, cell.y})) {
                continue;
            }
            const sf::Vector2i previous(cell.x - delta.x, cell.y - delta.y);
            if (!isDirectionPassable(previous, cell, direction)) {
                return false;
            }
        }
    }
    for (const sf::Vector2i& cell : occupied) {
        if (!self->getCollisionAt(cell.x, cell.y, const_cast<Actor&>(actor))
                 .empty()) {
            return false;
        }
    }
    return true;
}

std::vector<Actor*> GameMapBase::getCollision(Actor& actor,
                                              const sf::Vector2i& position) {
    if (!actor.getCollisionEnabled()) {
        return {};
    }
    ensurePassabilityCache();
    std::vector<Actor*> result;
    std::unordered_set<Actor*> seen;
    for (const sf::Vector2i& cell :
         actor.getOccupiedMapCellsAtMapPosition(position)) {
        for (Actor* other : getCollisionAt(cell.x, cell.y, actor)) {
            if (seen.insert(other).second) {
                result.push_back(other);
            }
        }
    }
    return result;
}

std::vector<Actor*> GameMapBase::getOverlaps(Actor& actor) {
    ensurePassabilityCache();
    std::vector<Actor*> result;
    std::unordered_set<Actor*> seen;
    for (const sf::Vector2i& cell : actor.getOccupiedMapCells()) {
        for (Actor* other : getOverlapsAt(cell.x, cell.y, actor)) {
            if (seen.insert(other).second) {
                result.push_back(other);
            }
        }
    }
    return result;
}

std::optional<Material> GameMapBase::getTopMaterial(
    const sf::Vector2i& position) const {
    for (const std::string& layerName : getTopFirstLayerNames()) {
        const std::shared_ptr<TileLayer> layer = tilemap_->getLayer(layerName);
        if (layer == nullptr || !layer->getVisible()) {
            continue;
        }
        auto actorLayerIt = materialActorsRef_.find(layerName);
        if (actorLayerIt != materialActorsRef_.end()) {
            for (const ActorPtr& actor : actorLayerIt->second) {
                if (actor && actor.get() != playerActor_.get() &&
                    actor->getMapPosition() == position) {
                    return actor->getMaterial();
                }
            }
        }
        const std::optional<Material> material = layer->getMaterial(position);
        if (material.has_value()) {
            return material;
        }
    }
    return std::nullopt;
}

void GameMapBase::updateActorList() {
    if (actorListUpdater_) {
        actorListUpdater_();
    }
    passabilityDirty_ = true;
}

void GameMapBase::destroyActor(Actor& actor) {
    if (actorDestroyer_) {
        actorDestroyer_(actor);
    }
    passabilityDirty_ = true;
}

void GameMapBase::setActorListUpdater(std::function<void()> updater) {
    actorListUpdater_ = std::move(updater);
}

void GameMapBase::setActorDestroyer(std::function<void(Actor&)> destroyer) {
    actorDestroyer_ = std::move(destroyer);
}

void GameMapBase::setPlayerActor(ActorPtr actor) {
    playerActor_ = std::move(actor);
}

std::vector<Actor*> GameMapBase::getActorsAt(int x, int y) {
    auto it = occupancyMap_.find({x, y});
    if (it == occupancyMap_.end()) {
        return {};
    }
    std::vector<Actor*> result;
    result.reserve(it->second.size());
    for (Actor* actor : it->second) {
        result.push_back(actor);
    }
    return result;
}

std::vector<Actor*> GameMapBase::getActorsInRange(int x, int y, int radius) {
    return getActorsInRangeImpl(x, y, radius, nullptr);
}

std::vector<Actor*> GameMapBase::getActorsInRangeExcluding(
    int x, int y, int radius, Actor& excludedActor) {
    return getActorsInRangeImpl(x, y, radius, &excludedActor);
}

std::vector<Actor*> GameMapBase::getActorsInRangeImpl(
    int x, int y, int radius, const Actor* excludedActor) {
    std::vector<Actor*> result;
    std::unordered_set<Actor*> seen;
    for (int ix = x - radius; ix <= x + radius; ++ix) {
        for (int iy = y - radius; iy <= y + radius; ++iy) {
            auto it = occupancyMap_.find({ix, iy});
            if (it == occupancyMap_.end()) {
                continue;
            }
            for (Actor* actor : it->second) {
                if (actor == excludedActor) {
                    continue;
                }
                if (seen.insert(actor).second) {
                    result.push_back(actor);
                }
            }
        }
    }
    return result;
}

std::vector<Actor*> GameMapBase::getCollisionAt(int x, int y,
                                                Actor& selfActor) {
    if (!selfActor.getCollisionEnabled()) {
        return {};
    }
    auto it = occupancyMap_.find({x, y});
    if (it == occupancyMap_.end()) {
        return {};
    }
    const auto& actorsAtCell = it->second;
    const int topmostLayerIndex =
        getTopmostOccupantLayerIndex(actorsAtCell, &selfActor);
    if (topmostLayerIndex == std::numeric_limits<int>::max()) {
        return {};
    }
    const auto& descendantActors = selfActor.getDescendantActors();
    std::vector<Actor*> result;
    for (Actor* otherActor : actorsAtCell) {
        if (otherActor == &selfActor) {
            continue;
        }
        if (descendantActors.find(otherActor) != descendantActors.end()) {
            continue;
        }
        if (otherActor->isDestroyed()) {
            continue;
        }
        if (getActorLayerIndex(otherActor) != topmostLayerIndex) {
            continue;
        }
        if (!otherActor->getCollisionEnabled()) {
            continue;
        }
        result.push_back(otherActor);
    }
    return result;
}

std::vector<Actor*> GameMapBase::getOverlapsAt(int x, int y, Actor& selfActor) {
    auto it = occupancyMap_.find({x, y});
    if (it == occupancyMap_.end()) {
        return {};
    }
    const auto& actorsAtCell = it->second;
    const int topmostLayerIndex =
        getTopmostOccupantLayerIndex(actorsAtCell, &selfActor);
    if (topmostLayerIndex == std::numeric_limits<int>::max()) {
        return {};
    }
    const auto& descendantActors = selfActor.getDescendantActors();
    std::vector<Actor*> result;
    for (Actor* otherActor : actorsAtCell) {
        if (otherActor == &selfActor) {
            continue;
        }
        if (descendantActors.find(otherActor) != descendantActors.end()) {
            continue;
        }
        if (otherActor->isDestroyed()) {
            continue;
        }
        if (getActorLayerIndex(otherActor) != topmostLayerIndex) {
            continue;
        }
        result.push_back(otherActor);
    }
    return result;
}

bool GameMapBase::passable(int x, int y, int sx, int sy, int gx, int gy) {
    return passableForActor(x, y, sx, sy, gx, gy, nullptr);
}

bool GameMapBase::passableForActor(int x, int y, int sx, int sy, int gx, int gy,
                                   const Actor* excludedActor) {
    if ((x == sx && y == sy) || (x == gx && y == gy)) {
        return true;
    }
    auto occupancyIt = occupancyMap_.find({x, y});
    if (occupancyIt != occupancyMap_.end()) {
        const int topmostLayerIndex =
            getTopmostOccupantLayerIndex(occupancyIt->second, excludedActor);
        const std::unordered_set<Actor*>* descendantActors = nullptr;
        if (excludedActor != nullptr) {
            descendantActors = &excludedActor->getDescendantActors();
        }
        if (topmostLayerIndex != std::numeric_limits<int>::max()) {
            for (Actor* actor : occupancyIt->second) {
                if (actor == excludedActor) {
                    continue;
                }
                if (descendantActors != nullptr &&
                    descendantActors->find(actor) != descendantActors->end()) {
                    continue;
                }
                if (actor->isDestroyed()) {
                    continue;
                }
                if (getActorLayerIndex(actor) != topmostLayerIndex) {
                    continue;
                }
                if (actor->blocksPassability()) {
                    return false;
                }
            }
        }
    }
    const sf::Vector2i position(x, y);
    for (const std::string& layerName : getTopFirstLayerNames()) {
        const std::shared_ptr<TileLayer> layer = tilemap_->getLayer(layerName);
        if (!layer || !layer->getVisible() || !layer->hasContent(position)) {
            continue;
        }
        return layer->isPassable(position);
    }
    return true;
}

int GameMapBase::getActorLayerIndex(const Actor* actor) const {
    if (actor == nullptr) {
        return std::numeric_limits<int>::max();
    }
    auto layerIt = actorLayerRef_.find(const_cast<Actor*>(actor));
    if (layerIt == actorLayerRef_.end()) {
        return std::numeric_limits<int>::max();
    }
    const std::vector<std::string> layerNames = getTopFirstLayerNames();
    const auto layerIndexIt =
        std::find(layerNames.begin(), layerNames.end(), layerIt->second);
    if (layerIndexIt == layerNames.end()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(layerIndexIt - layerNames.begin());
}

int GameMapBase::getTopmostOccupantLayerIndex(
    const std::vector<Actor*>& actorsAtCell, const Actor* selfActor) const {
    const std::unordered_set<Actor*>* descendantActors = nullptr;
    if (selfActor != nullptr) {
        descendantActors = &selfActor->getDescendantActors();
    }
    int topmostLayerIndex = std::numeric_limits<int>::max();
    for (Actor* actor : actorsAtCell) {
        if (selfActor != nullptr && actor == selfActor) {
            continue;
        }
        if (descendantActors != nullptr &&
            descendantActors->find(actor) != descendantActors->end()) {
            continue;
        }
        if (actor->isDestroyed()) {
            continue;
        }
        const int layerIndex = getActorLayerIndex(actor);
        if (layerIndex < topmostLayerIndex) {
            topmostLayerIndex = layerIndex;
        }
    }
    return topmostLayerIndex;
}

void GameMapBase::registerActorOccupancy(Actor& actor) {
    const std::vector<sf::Vector2i> cells = actor.getOccupiedMapCells();
    registeredOccupancyCells_[&actor] = cells;
    for (const sf::Vector2i& cell : cells) {
        auto key = std::make_pair(cell.x, cell.y);
        auto& actorsAtCell = occupancyMap_[key];
        if (std::find(actorsAtCell.begin(), actorsAtCell.end(), &actor) ==
            actorsAtCell.end()) {
            actorsAtCell.push_back(&actor);
        }
    }
}

void GameMapBase::unregisterActorOccupancy(Actor& actor) {
    auto registeredIt = registeredOccupancyCells_.find(&actor);
    if (registeredIt != registeredOccupancyCells_.end()) {
        for (const sf::Vector2i& cell : registeredIt->second) {
            auto key = std::make_pair(cell.x, cell.y);
            auto occupancyIt = occupancyMap_.find(key);
            if (occupancyIt == occupancyMap_.end()) {
                continue;
            }
            auto& actorsAtCell = occupancyIt->second;
            actorsAtCell.erase(
                std::remove(actorsAtCell.begin(), actorsAtCell.end(), &actor),
                actorsAtCell.end());
            if (actorsAtCell.empty()) {
                occupancyMap_.erase(occupancyIt);
            }
        }
        registeredOccupancyCells_.erase(registeredIt);
        return;
    }
    for (auto it = occupancyMap_.begin(); it != occupancyMap_.end();) {
        auto& actorsAtCell = it->second;
        actorsAtCell.erase(
            std::remove(actorsAtCell.begin(), actorsAtCell.end(), &actor),
            actorsAtCell.end());
        if (actorsAtCell.empty()) {
            it = occupancyMap_.erase(it);
        } else {
            ++it;
        }
    }
}

void GameMapBase::updateActorOccupancy(Actor& actor) {
    ensurePassabilityCache();
    unregisterActorOccupancy(actor);
    registerActorOccupancy(actor);
}

void GameMapBase::ensurePassabilityCache() const {
    const sf::Vector2u size = getSize();
    const bool wrongHeight = tilePassableGrid_.size() != size.y;
    const bool wrongWidth = !tilePassableGrid_.empty() &&
                            tilePassableGrid_.front().size() != size.x;
    if (!passabilityDirty_ && !wrongHeight && !wrongWidth) {
        return;
    }
    GameMapBase* self = const_cast<GameMapBase*>(this);
    self->rebuildPassabilityCache(size);
}

void GameMapBase::refreshActorOccupancyCache() {
    std::unordered_set<Actor*> seenActors;
    for (const auto& [_, actorList] : actorsRef_) {
        for (const ActorPtr& actor : actorList) {
            if (!actor || !seenActors.insert(actor.get()).second) {
                continue;
            }
            const auto registeredIt =
                registeredOccupancyCells_.find(actor.get());
            if (actor->isDestroyed()) {
                if (registeredIt != registeredOccupancyCells_.end()) {
                    unregisterActorOccupancy(*actor);
                }
                continue;
            }
            const std::vector<sf::Vector2i> occupiedCells =
                actor->getOccupiedMapCells();
            if (registeredIt != registeredOccupancyCells_.end() &&
                registeredIt->second == occupiedCells) {
                continue;
            }
            if (registeredIt != registeredOccupancyCells_.end()) {
                unregisterActorOccupancy(*actor);
            }
            registerActorOccupancy(*actor);
        }
    }
}

bool GameMapBase::nodePassableForActor(int x, int y, int sx, int sy, int gx,
                                       int gy, unsigned int width,
                                       unsigned int height,
                                       const Actor& movingActor) {
    const std::vector<sf::Vector2i> cells =
        movingActor.getOccupiedMapCellsAtMapPosition({x, y});
    if (cells.empty()) {
        return passable(x, y, sx, sy, gx, gy);
    }
    for (const sf::Vector2i& cell : cells) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= static_cast<int>(width) ||
            cell.y >= static_cast<int>(height)) {
            return false;
        }
        if (!passableForActor(cell.x, cell.y, sx, sy, gx, gy, &movingActor)) {
            return false;
        }
    }
    return true;
}

bool GameMapBase::isDirectionPassable(const sf::Vector2i& fromPosition,
                                      const sf::Vector2i& toPosition,
                                      int direction) const {
    const int opposite = oppositeDirection(direction);
    bool fromFound = false;
    bool toFound = false;
    for (const std::string& layerName : getTopFirstLayerNames()) {
        const std::shared_ptr<TileLayer> layer = tilemap_->getLayer(layerName);
        if (layer == nullptr || !layer->getVisible()) {
            continue;
        }
        if (!fromFound && layer->get(fromPosition).has_value()) {
            if (!layer->isDirectionPassable(fromPosition, direction)) {
                return false;
            }
            fromFound = true;
        }
        if (!toFound && layer->get(toPosition).has_value()) {
            if (!layer->isDirectionPassable(toPosition, opposite)) {
                return false;
            }
            toFound = true;
        }
        if (fromFound && toFound) {
            break;
        }
    }
    return true;
}

std::vector<std::string> GameMapBase::getTopFirstLayerNames() const {
    if (!tilemap_) {
        return {};
    }
    std::vector<std::string> layerNames = tilemap_->getLayerNameList();
    std::reverse(layerNames.begin(), layerNames.end());
    return layerNames;
}

MaterialValue GameMapBase::getMaterialProperty(
    const sf::Vector2i& pos, const std::string& propertyName,
    const MaterialValue& invalidValue) const {
    for (const std::string& layerName : getTopFirstLayerNames()) {
        const std::shared_ptr<TileLayer> layer = tilemap_->getLayer(layerName);
        if (!layer || !layer->getVisible() || !layer->hasContent(pos)) {
            continue;
        }
        const std::optional<MaterialValue> value =
            layer->getMaterialProperty(pos, propertyName);
        return value.value_or(invalidValue);
    }
    return invalidValue;
}
