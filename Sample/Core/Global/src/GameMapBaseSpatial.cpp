#include "GameMapBase.hpp"

#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <unordered_set>
#include <utility>

std::size_t IntPairHash::operator()(const IntPair& value) const {
    std::size_t xHash = std::hash<int>{}(value.first);
    std::size_t yHash = std::hash<int>{}(value.second);
    return xHash ^ (yHash + 0x9e3779b9 + (xHash << 6) + (xHash >> 2));
}

void GameMapBase::configureSparseWorld(
    const sf::Vector2u& size, const std::vector<std::string>& layerOrder,
    std::function<bool(const sf::Vector2i&)> tilePassabilityQuery,
    std::function<bool(const sf::Vector2i&, const sf::Vector2i&, int)>
        directionPassabilityQuery,
    std::function<std::optional<Material>(const sf::Vector2i&)>
        topMaterialQuery) {
    if (!tilePassabilityQuery || !directionPassabilityQuery ||
        !topMaterialQuery) {
        throw std::invalid_argument(
            "Sparse world queries must all be configured");
    }
    sparseWorldSize_ = size;
    sparseWorldLayerOrder_ = layerOrder;
    sparseTilePassabilityQuery_ = std::move(tilePassabilityQuery);
    sparseDirectionPassabilityQuery_ = std::move(directionPassabilityQuery);
    sparseTopMaterialQuery_ = std::move(topMaterialQuery);
    clearActorOccupancy();
    registeredOccupancyCells_.clear();
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

void GameMapBase::clearSparseWorld() {
    sparseWorldSize_.reset();
    sparseWorldLayerOrder_.clear();
    sparseTilePassabilityQuery_ = {};
    sparseDirectionPassabilityQuery_ = {};
    sparseTopMaterialQuery_ = {};
    clearActorOccupancy();
    registeredOccupancyCells_.clear();
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

std::size_t GameMapBase::getSparseOccupancyPageCount() const {
    return sparseOccupancyPages_.size();
}

int GameMapBase::getOccupancyPageCoordinate(int value) {
    if (value >= 0) {
        return value / OccupancyPageSize;
    }
    return -1 - (-(value + 1) / OccupancyPageSize);
}

int GameMapBase::getOccupancyPageOffset(int value) {
    return value - getOccupancyPageCoordinate(value) * OccupancyPageSize;
}

IntPair GameMapBase::getOccupancyPageKey(int x, int y) {
    return {getOccupancyPageCoordinate(x), getOccupancyPageCoordinate(y)};
}

std::size_t GameMapBase::getOccupancyPageCellIndex(int x, int y) {
    return static_cast<std::size_t>(getOccupancyPageOffset(y) *
                                        OccupancyPageSize +
                                    getOccupancyPageOffset(x));
}

void GameMapBase::clearActorOccupancy() {
    occupancyMap_.clear();
    sparseOccupancyPages_.clear();
}

const std::vector<Actor*>* GameMapBase::findActorsAtCell(int x, int y) const {
    if (sparseWorldSize_.has_value()) {
        const IntPair pageKey = getOccupancyPageKey(x, y);
        const std::size_t cellIndex = getOccupancyPageCellIndex(x, y);
        const auto pageIt = sparseOccupancyPages_.find(pageKey);
        if (pageIt == sparseOccupancyPages_.end()) {
            return nullptr;
        }
        const std::vector<Actor*>& actors = pageIt->second.cells[cellIndex];
        return actors.empty() ? nullptr : &actors;
    }
    const auto occupancyIt = occupancyMap_.find({x, y});
    return occupancyIt == occupancyMap_.end() ? nullptr : &occupancyIt->second;
}

std::vector<std::vector<bool>> GameMapBase::rebuildPassabilityCache(
    const sf::Vector2u& size) {
    if (sparseWorldSize_.has_value()) {
        clearActorOccupancy();
        registeredOccupancyCells_.clear();
        for (auto& [_, actorList] : actorsRef_) {
            for (const ActorPtr& actor : actorList) {
                if (actor && !actor->isDestroyed()) {
                    registerActorOccupancy(*actor);
                }
            }
        }
        tilePassableGrid_.clear();
        passabilityDirty_ = false;
        return {};
    }
    unsigned int width = size.x;
    unsigned int height = size.y;
    const std::vector<std::string> layerNames = getTopFirstLayerNames();
    std::vector<std::vector<bool>> tilePassableGrid(height);
    clearActorOccupancy();
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
    if (sparseWorldSize_.has_value()) {
        return *sparseWorldSize_;
    }
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
        const bool tilePassable =
            sparseWorldSize_.has_value()
                ? sparseTilePassabilityQuery_(cell)
                : tilePassableGrid_[static_cast<std::size_t>(cell.y)]
                                   [static_cast<std::size_t>(cell.x)];
        if (!tilePassable) {
            return false;
        }
    }
    if (!directionPassableForActor(actor.getMapPosition(), position, occupied,
                                   actor)) {
        return false;
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
    if (sparseWorldSize_.has_value()) {
        return sparseTopMaterialQuery_(position);
    }
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

std::vector<Actor*> GameMapBase::getActorsAt(int x, int y) {
    const std::vector<Actor*>* actors = findActorsAtCell(x, y);
    if (actors == nullptr) {
        return {};
    }
    return *actors;
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
    const auto appendActors = [&](const std::vector<Actor*>& actors) {
        for (Actor* actor : actors) {
            if (actor == excludedActor) {
                continue;
            }
            if (seen.insert(actor).second) {
                result.push_back(actor);
            }
        }
    };
    if (sparseWorldSize_.has_value()) {
        for (int ix = x - radius; ix <= x + radius; ++ix) {
            std::optional<IntPair> currentPageKey;
            const SparseOccupancyPage* currentPage = nullptr;
            for (int iy = y - radius; iy <= y + radius; ++iy) {
                const IntPair pageKey = getOccupancyPageKey(ix, iy);
                if (!currentPageKey.has_value() || *currentPageKey != pageKey) {
                    currentPageKey = pageKey;
                    const auto pageIt = sparseOccupancyPages_.find(pageKey);
                    currentPage = pageIt == sparseOccupancyPages_.end()
                                      ? nullptr
                                      : &pageIt->second;
                }
                if (currentPage == nullptr) {
                    continue;
                }
                const std::vector<Actor*>& actors =
                    currentPage->cells[getOccupancyPageCellIndex(ix, iy)];
                appendActors(actors);
            }
        }
        return result;
    }
    for (int ix = x - radius; ix <= x + radius; ++ix) {
        for (int iy = y - radius; iy <= y + radius; ++iy) {
            auto it = occupancyMap_.find({ix, iy});
            if (it == occupancyMap_.end()) {
                continue;
            }
            appendActors(it->second);
        }
    }
    return result;
}

std::vector<Actor*> GameMapBase::getCollisionAt(int x, int y,
                                                Actor& selfActor) {
    if (!selfActor.getCollisionEnabled()) {
        return {};
    }
    const std::vector<Actor*>* actorsAtCell = findActorsAtCell(x, y);
    if (actorsAtCell == nullptr) {
        return {};
    }
    const int topmostLayerIndex =
        getTopmostOccupantLayerIndex(*actorsAtCell, &selfActor);
    if (topmostLayerIndex == std::numeric_limits<int>::max()) {
        return {};
    }
    const auto& descendantActors = selfActor.getDescendantActors();
    std::vector<Actor*> result;
    for (auto actorIt = actorsAtCell->rbegin(); actorIt != actorsAtCell->rend();
         ++actorIt) {
        Actor* otherActor = *actorIt;
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
    const std::vector<Actor*>* actorsAtCell = findActorsAtCell(x, y);
    if (actorsAtCell == nullptr) {
        return {};
    }
    const int topmostLayerIndex =
        getTopmostOccupantLayerIndex(*actorsAtCell, &selfActor);
    if (topmostLayerIndex == std::numeric_limits<int>::max()) {
        return {};
    }
    const auto& descendantActors = selfActor.getDescendantActors();
    std::vector<Actor*> result;
    for (Actor* otherActor : *actorsAtCell) {
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
    const std::vector<Actor*>* actorsAtCell = findActorsAtCell(x, y);
    if (actorsAtCell != nullptr) {
        const int topmostLayerIndex =
            getTopmostOccupantLayerIndex(*actorsAtCell, excludedActor);
        const std::unordered_set<Actor*>* descendantActors = nullptr;
        if (excludedActor != nullptr) {
            descendantActors = &excludedActor->getDescendantActors();
        }
        if (topmostLayerIndex != std::numeric_limits<int>::max()) {
            for (Actor* actor : *actorsAtCell) {
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
    if (sparseWorldSize_.has_value()) {
        return sparseTilePassabilityQuery_(position);
    }
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
        if (sparseWorldSize_.has_value()) {
            const IntPair pageKey = getOccupancyPageKey(cell.x, cell.y);
            const std::size_t cellIndex =
                getOccupancyPageCellIndex(cell.x, cell.y);
            SparseOccupancyPage& page = sparseOccupancyPages_[pageKey];
            std::vector<Actor*>& actorsAtCell = page.cells[cellIndex];
            if (std::find(actorsAtCell.begin(), actorsAtCell.end(), &actor) ==
                actorsAtCell.end()) {
                if (actorsAtCell.empty()) {
                    ++page.occupiedCellCount;
                }
                actorsAtCell.push_back(&actor);
            }
            continue;
        }
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
            if (sparseWorldSize_.has_value()) {
                const IntPair pageKey = getOccupancyPageKey(cell.x, cell.y);
                auto pageIt = sparseOccupancyPages_.find(pageKey);
                if (pageIt == sparseOccupancyPages_.end()) {
                    continue;
                }
                SparseOccupancyPage& page = pageIt->second;
                std::vector<Actor*>& actorsAtCell =
                    page.cells[getOccupancyPageCellIndex(cell.x, cell.y)];
                const bool wasOccupied = !actorsAtCell.empty();
                actorsAtCell.erase(std::remove(actorsAtCell.begin(),
                                               actorsAtCell.end(), &actor),
                                   actorsAtCell.end());
                if (wasOccupied && actorsAtCell.empty()) {
                    --page.occupiedCellCount;
                    if (page.occupiedCellCount == 0) {
                        sparseOccupancyPages_.erase(pageIt);
                    }
                }
                continue;
            }
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
    if (sparseWorldSize_.has_value()) {
        for (auto pageIt = sparseOccupancyPages_.begin();
             pageIt != sparseOccupancyPages_.end();) {
            SparseOccupancyPage& page = pageIt->second;
            for (std::vector<Actor*>& actorsAtCell : page.cells) {
                const bool wasOccupied = !actorsAtCell.empty();
                actorsAtCell.erase(std::remove(actorsAtCell.begin(),
                                               actorsAtCell.end(), &actor),
                                   actorsAtCell.end());
                if (wasOccupied && actorsAtCell.empty()) {
                    --page.occupiedCellCount;
                }
            }
            if (page.occupiedCellCount == 0) {
                pageIt = sparseOccupancyPages_.erase(pageIt);
            } else {
                ++pageIt;
            }
        }
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
    if (sparseWorldSize_.has_value()) {
        if (!passabilityDirty_) {
            return;
        }
        GameMapBase* self = const_cast<GameMapBase*>(this);
        self->rebuildPassabilityCache(*sparseWorldSize_);
        return;
    }
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

std::vector<std::string> GameMapBase::getTopFirstLayerNames() const {
    if (sparseWorldSize_.has_value()) {
        std::vector<std::string> layerNames = sparseWorldLayerOrder_;
        std::reverse(layerNames.begin(), layerNames.end());
        return layerNames;
    }
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
    if (sparseWorldSize_.has_value()) {
        const std::optional<Material> material = sparseTopMaterialQuery_(pos);
        if (!material.has_value()) {
            return invalidValue;
        }
        std::string fieldName = propertyName;
        if (propertyName == "getLightBlock") {
            fieldName = "lightBlock";
        } else if (propertyName == "getMirror") {
            fieldName = "mirror";
        } else if (propertyName == "getReflectionStrength") {
            fieldName = "reflectionStrength";
        } else if (propertyName == "getIgnoreLighting") {
            fieldName = "ignoreLighting";
        } else if (propertyName == "getSpeedRate") {
            fieldName = "speedRate";
        }
        const MaterialData values = material->asDict();
        const auto iterator = values.find(fieldName);
        return iterator == values.end() ? invalidValue : iterator->second;
    }
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
