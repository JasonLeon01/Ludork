#include "GameMapBase/ActorRegistryImpl.hpp"
#include "GameMapBase/RegionVisibilityImpl.hpp"
#include "GameMapBase/LightOcclusionImpl.hpp"
#include "GameMapBase/OccupancyIndexImpl.hpp"
#include "GameMapBase/SparseWorldImpl.hpp"
#include "GameMapBase/GridPointHash.hpp"
#include "GameMapBase/PathfindingImpl.hpp"
#include <Gameplay/ActorMapService.hpp>
#include <Gameplay/TileLayer.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>
#include <LightOcclusionInput.hpp>
#include <LightOcclusionResult.hpp>
#include <GameMapBase.hpp>
#include <Camera.hpp>
#include <Gameplay/Components/BillboardComponent.hpp>
#include <Emitters/EmitterScheduler.hpp>
#include <Runtime/RuntimeObject.hpp>

#include <Gameplay/Actor.hpp>
#include <EngineState.hpp>

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace {

bool intersectsCamera(const sf::FloatRect& bounds,
                      const sf::Transform& clipTransform) {
    const sf::Vector2f end = bounds.position + bounds.size;
    const std::array<sf::Vector2f, 4> corners = {
        clipTransform.transformPoint(bounds.position),
        clipTransform.transformPoint({end.x, bounds.position.y}),
        clipTransform.transformPoint(end),
        clipTransform.transformPoint({bounds.position.x, end.y})};
    const auto separated = [&](sf::Vector2f axis) {
        float minimum = std::numeric_limits<float>::max();
        float maximum = std::numeric_limits<float>::lowest();
        for (const sf::Vector2f& corner : corners) {
            const float projection = corner.x * axis.x + corner.y * axis.y;
            minimum = std::min(minimum, projection);
            maximum = std::max(maximum, projection);
        }
        const float clipExtent = std::abs(axis.x) + std::abs(axis.y);
        return minimum > clipExtent || maximum < -clipExtent;
    };
    if (separated({1.0f, 0.0f}) || separated({0.0f, 1.0f})) {
        return false;
    }
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const sf::Vector2f edge =
            corners[(index + 1) % corners.size()] - corners[index];
        if (separated({-edge.y, edge.x})) {
            return false;
        }
    }
    return true;
}

float materialValueToFloat(const MaterialValue& value) {
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

}  // namespace

GameMapBase::GameMapBase()
    : regionVisibility_(
          std::make_unique<
              ludork::global::game_map_base_impl::RegionVisibilityImpl>()),
      sparseWorld_(std::make_unique<
                   ludork::global::game_map_base_impl::SparseWorldImpl>()),
      lightOcclusion_(
          std::make_unique<
              ludork::global::game_map_base_impl::LightOcclusionImpl>()),
      occupancy_(std::make_unique<
                 ludork::global::game_map_base_impl::OccupancyIndexImpl>()),
      actorRegistry_(std::make_unique<
                     ludork::global::game_map_base_impl::ActorRegistryImpl>()) {
}

GameMapBase::~GameMapBase() {
    releaseEmitters();
    releaseBillboards();
}

void GameMapBase::collectEmitters(EmitterScheduler& scheduler) {
    for (const auto& [_, actors] : actorRegistry_->materialActors()) {
        for (const ActorPtr& actor : actors) {
            if (actor != nullptr) {
                actor->collectEmitter(scheduler);
            }
        }
    }
}

void GameMapBase::releaseEmitters() noexcept {
    actorRegistry_->releaseEmitters();
}

void GameMapBase::releaseBillboards() noexcept {
    actorRegistry_->releaseBillboards();
}

void GameMapBase::_updateBillboards(
    float deltaTime, const Camera& camera,
    std::function<bool(Actor&, const std::string&)> layerVisible) {
    const ActorPtr& player = actorRegistry_->playerActor();
    const sf::Transform clipTransform =
        camera.getView().getTransform() * camera.getRenderStates().transform;
    std::unordered_map<Actor*, bool> presentationVisible;
    for (const auto& [layerName, actors] : actorRegistry_->materialActors()) {
        for (const ActorPtr& actor : actors) {
            if (!actor || !actor->getBillboardComponent()) {
                continue;
            }
            bool& visible = presentationVisible[actor.get()];
            visible = visible || (isActorVisibleOnMap(*actor) &&
                                  layerVisible(*actor, layerName) &&
                                  intersectsCamera(actor->getGlobalBounds(),
                                                   clipTransform));
        }
    }
    for (const auto& [actor, visible] : presentationVisible) {
        const float range =
            std::max(0.0f, actor->getBillboardComponent()->showRange);
        bool inRange = false;
        if (player && !player->isDestroyed()) {
            const sf::Vector2f distance =
                actor->getPosition() - player->getPosition();
            inRange = distance.lengthSquared() <= range * range;
        }
        actor->updateBillboard(deltaTime, visible, inRange);
    }
}

const ActorDict& GameMapBase::getMaterialActorsForRenderer() const {
    return actorRegistry_->materialActors();
}

const ActorPtr& GameMapBase::getPlayerActorForRenderer() const {
    return actorRegistry_->playerActor();
}

void GameMapBase::setTilemap(std::shared_ptr<Tilemap> tilemap) {
    tilemap_ = std::move(tilemap);
    visibilityDirty_ = true;
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
    actorRegistry_->syncActorsRef(actors, *occupancy_);
    passabilityDirty_ = true;
}

void GameMapBase::syncMaterialActorsRef(const ActorDict& actors) {
    actorRegistry_->syncMaterialActorsRef(actors);
}

bool GameMapBase::registerLayerActor(ActorPtr actor, const std::string& layer) {
    return actorRegistry_->registerLayerActor(std::move(actor), layer);
}

std::optional<std::string> GameMapBase::getRegisteredActorLayer(
    Actor& actor) const {
    return actorRegistry_->getRegisteredActorLayer(actor);
}

void GameMapBase::beginActorBatch() {
    actorRegistry_->beginActorBatch();
}

void GameMapBase::endActorBatch() {
    actorRegistry_->endActorBatch();
}

void GameMapBase::flushActorChanges() {
    actorRegistry_->flushActorChanges();
}

void GameMapBase::withDeferredActorViewSync(std::function<void()> handler) {
    passabilityDirty_ = true;
    actorRegistry_->withDeferredActorViewSync(std::move(handler));
}

void GameMapBase::drainActorLifecycle(
    std::function<void(Actor&)> createHandler,
    std::function<void(Actor&)> componentHandler) {
    actorRegistry_->drainActorLifecycle(std::move(createHandler),
                                        std::move(componentHandler));
}

void GameMapBase::syncActorViews(const ActorDict& actors) {
    std::shared_ptr<RuntimeObject> stableOwner = runtimeOwner();
    if (!stableOwner) {
        stableOwner = weak_from_this().lock();
    }
    if (!stableOwner) {
        throw std::logic_error("Game map has no stable shared owner");
    }
    const std::shared_ptr<ActorMapService> mapOwner(
        stableOwner, static_cast<ActorMapService*>(this));
    actorRegistry_->syncActorViews(actors, mapOwner, *occupancy_);
    passabilityDirty_ = true;
}

void GameMapBase::forgetActors(const std::vector<ActorPtr>& actors) {
    if (actorRegistry_->forgetActors(actors, *occupancy_,
                                     sparseWorld_->size())) {
        passabilityDirty_ = true;
    }
}

void GameMapBase::updateActors(float deltaTime) {
    actorRegistry_->updateActors(deltaTime);
}

void GameMapBase::lateUpdateActors(float deltaTime) {
    actorRegistry_->lateUpdateActors(deltaTime);
}

void GameMapBase::fixedUpdateActors(float fixedDelta) {
    actorRegistry_->fixedUpdateActors(fixedDelta);
}

void GameMapBase::updateActorList() {
    actorRegistry_->markViewsDirty();
    passabilityDirty_ = true;
    actorRegistry_->flushActorChanges();
}

void GameMapBase::destroyActor(Actor& actor) {
    actorRegistry_->destroyActor(actor);
    passabilityDirty_ = true;
}

void GameMapBase::setActorListUpdater(std::function<void()> updater) {
    actorRegistry_->setActorListUpdater(std::move(updater));
}

void GameMapBase::setActorDestroyer(std::function<void(Actor&)> destroyer) {
    actorRegistry_->setActorDestroyer(std::move(destroyer));
}

void GameMapBase::setPlayerActor(ActorPtr actor) {
    actorRegistry_->setPlayerActor(std::move(actor));
}

std::shared_ptr<sf::Texture> GameMapBase::rebuildStaticLightOccupancy(
    const sf::Vector2i& origin, const sf::Vector2u& size,
    const std::vector<std::shared_ptr<Actor>>& actors) {
    return lightOcclusion_->rebuildStaticLightOccupancy(
        origin, size, actors, *sparseWorld_, tilemap_, EngineState::CellSize);
}

std::vector<LightOcclusionResult> GameMapBase::analyseLightOcclusion(
    const std::vector<LightOcclusionInput>& inputs,
    const std::vector<std::shared_ptr<Actor>>& visibleActors) {
    ensurePassabilityCache();
    return lightOcclusion_->analyseLightOcclusion(
        inputs, visibleActors, *occupancy_, sparseWorld_->size(),
        EngineState::CellSize);
}

GameMapBase::PathResult GameMapBase::findPathExt(
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
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    std::vector<IntPair> excludedAnchorSet;
    excludedAnchorSet.reserve(excludedAnchors.size());
    for (const sf::Vector2i& anchor : excludedAnchors) {
        excludedAnchorSet.emplace_back(anchor.x, anchor.y);
    }
    GameMapBase::PathResult result;
    if (start_t == goal_t) {
        result.route.emplace_back(sx, sy);
        return result;
    }
    const std::vector<IntPair> pathPosition =
        ludork::global::game_map_base_impl::findPath(
            start_t, goal_t, width, height, excludedAnchorSet,
            [this, sx, sy, gx, gy, width, height, &movingActor](
                int fromX, int fromY, int toX, int toY) {
                return transitionPassableForActor(fromX, fromY, toX, toY, sx,
                                                  sy, gx, gy, width, height,
                                                  movingActor);
            });
    if (!pathPosition.empty()) {
        result.offsets.reserve(pathPosition.size());
        result.points.reserve(pathPosition.size());
        result.route.reserve(pathPosition.size() + 1);
        result.route.emplace_back(sx, sy);
        int px = sx;
        int py = sy;
        for (const auto& [x, y] : pathPosition) {
            result.offsets.emplace_back(x - px, y - py);
            result.points.emplace_back(x, y);
            result.route.emplace_back(x, y);
            px = x;
            py = y;
        }
    }
    return result;
}

bool GameMapBase::transitionPassableForActor(int fromX, int fromY, int x, int y,
                                             int sx, int sy, int gx, int gy,
                                             unsigned int width,
                                             unsigned int height,
                                             const Actor& movingActor) {
    const std::vector<sf::Vector2i> cells =
        movingActor.getOccupiedMapCellsAtMapPosition({x, y});
    if (cells.empty()) {
        return passable(x, y, sx, sy, gx, gy) &&
               directionPassableForActor({fromX, fromY}, {x, y}, cells,
                                         movingActor);
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
    return directionPassableForActor({fromX, fromY}, {x, y}, cells,
                                     movingActor);
}

bool GameMapBase::directionPassableForActor(
    const sf::Vector2i& fromPosition, const sf::Vector2i& toPosition,
    const std::vector<sf::Vector2i>& toCells, const Actor& movingActor) const {
    const sf::Vector2i delta = toPosition - fromPosition;
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
    if (direction < 0) {
        return true;
    }
    const std::vector<sf::Vector2i> fromCells =
        movingActor.getOccupiedMapCellsAtMapPosition(fromPosition);
    std::unordered_set<IntPair,
                       ludork::global::game_map_base_impl::GridPointHash>
        fromCellSet;
    fromCellSet.reserve(fromCells.size());
    for (const sf::Vector2i& cell : fromCells) {
        fromCellSet.emplace(cell.x, cell.y);
    }
    for (const sf::Vector2i& cell : toCells) {
        if (fromCellSet.contains({cell.x, cell.y})) {
            continue;
        }
        const sf::Vector2i previous(cell.x - delta.x, cell.y - delta.y);
        if (!isDirectionPassable(previous, cell, direction)) {
            return false;
        }
    }
    return true;
}

bool GameMapBase::isDirectionPassable(const sf::Vector2i& fromPosition,
                                      const sf::Vector2i& toPosition,
                                      int direction) const {
    if (sparseWorld_->size().has_value()) {
        return sparseWorld_->isSparseWorldDirectionPassable(
            fromPosition, toPosition, direction);
    }
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

void GameMapBase::configureSparseWorld(
    const sf::Vector2u& size, const std::vector<std::string>& layerOrder,
    const std::vector<sf::IntRect>& regionRects) {
    if (hideDisconnectedRegions_) {
        throw std::invalid_argument(
            "Disconnected-region hiding requires an ordinary tile map");
    }
    sparseWorld_->configureSparseWorld(size, layerOrder, regionRects);
    lightOcclusion_->clearStaticLightOccupancy();
    occupancy_->clearActorOccupancy();
    occupancy_->clearRegisteredCells();
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

void GameMapBase::setSparseWorldRegion(int regionIndex,
                                       std::shared_ptr<Tilemap> tilemap,
                                       bool actorsReady) {
    sparseWorld_->setSparseWorldRegion(regionIndex, std::move(tilemap),
                                       actorsReady);
    lightOcclusion_->clearStaticLightOccupancy();
}

void GameMapBase::setSparseWorldRegionActorsReady(int regionIndex) {
    sparseWorld_->setSparseWorldRegionActorsReady(regionIndex);
}

void GameMapBase::detachSparseWorldRegion(int regionIndex) {
    if (sparseWorld_->detachSparseWorldRegion(regionIndex)) {
        lightOcclusion_->clearStaticLightOccupancy();
    }
}

void GameMapBase::setSparseWorldPreparedRect(std::optional<sf::IntRect> rect) {
    sparseWorld_->setSparseWorldPreparedRect(rect);
}

bool GameMapBase::isSparseWorldCellReady(const sf::Vector2i& position) const {
    return sparseWorld_->isSparseWorldCellReady(position);
}

bool GameMapBase::isSparseWorldGameplayPositionReady(
    const sf::Vector2i& position) const {
    return sparseWorld_->isSparseWorldGameplayPositionReady(position);
}

std::optional<int> GameMapBase::getSparseWorldRegionIndexAt(
    const sf::Vector2i& position) const {
    return sparseWorld_->getSparseWorldRegionIndexAt(position);
}

void GameMapBase::clearSparseWorld() {
    sparseWorld_->clearSparseWorld();
    lightOcclusion_->clearStaticLightOccupancy();
    occupancy_->clearActorOccupancy();
    occupancy_->clearRegisteredCells();
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

std::size_t GameMapBase::getSparseOccupancyPageCount() const {
    return occupancy_->getSparseOccupancyPageCount();
}

std::vector<std::vector<bool>> GameMapBase::rebuildPassabilityCache(
    const sf::Vector2u& size) {
    if (sparseWorld_->size().has_value()) {
        occupancy_->clearActorOccupancy();
        occupancy_->clearRegisteredCells();
        for (auto& [_, actorList] : actorRegistry_->actors()) {
            for (const ActorPtr& actor : actorList) {
                if (actor && !actor->isDestroyed()) {
                    occupancy_->registerActorOccupancy(*actor,
                                                       sparseWorld_->size());
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
    occupancy_->clearActorOccupancy();
    occupancy_->clearRegisteredCells();
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
    for (auto& [_, actorList] : actorRegistry_->actors()) {
        for (const ActorPtr& actor : actorList) {
            occupancy_->registerActorOccupancy(*actor, sparseWorld_->size());
        }
    }
    tilePassableGrid_ = tilePassableGrid;
    passabilityDirty_ = false;
    return tilePassableGrid_;
}

void GameMapBase::invalidatePassabilityCache() {
    visibilityDirty_ = true;
    passabilityDirty_ = true;
}

sf::Vector2u GameMapBase::getSize() const {
    if (sparseWorld_->size().has_value()) {
        return *sparseWorld_->size();
    }
    return tilemap_ == nullptr ? sf::Vector2u{} : tilemap_->getSize();
}

bool GameMapBase::isPassable(const Actor& actor,
                             const sf::Vector2i& position) const {
    if (!actor.getCollisionEnabled() || !actor.isVisibleInHierarchy()) {
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
            sparseWorld_->size().has_value()
                ? sparseWorld_->isSparseWorldTilePassable(cell)
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
    if (!actor.getCollisionEnabled() || !actor.isVisibleInHierarchy()) {
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
    if (!actor.isVisibleInHierarchy()) {
        return {};
    }
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
    if (sparseWorld_->size().has_value()) {
        return sparseWorld_->getSparseWorldTopMaterial(position,
                                                       *actorRegistry_);
    }
    for (const std::string& layerName : getTopFirstLayerNames()) {
        const std::shared_ptr<TileLayer> layer = tilemap_->getLayer(layerName);
        if (layer == nullptr || !layer->getVisible()) {
            continue;
        }
        auto actorLayerIt = actorRegistry_->materialActors().find(layerName);
        if (actorLayerIt != actorRegistry_->materialActors().end()) {
            for (const ActorPtr& actor : actorLayerIt->second) {
                if (actor && !actor->isDestroyed() &&
                    actor->isVisibleInHierarchy() &&
                    actor.get() != actorRegistry_->playerActor().get() &&
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
    ensurePassabilityCache();
    const std::vector<Actor*>* actors =
        occupancy_->findActorsAtCell(x, y, sparseWorld_->size());
    if (actors == nullptr) {
        return {};
    }
    std::vector<Actor*> result;
    for (Actor* actor : *actors) {
        if (!actor->isDestroyed() && actor->isVisibleInHierarchy()) {
            result.push_back(actor);
        }
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
    ensurePassabilityCache();
    return occupancy_->getActorsInRangeImpl(x, y, radius, excludedActor,
                                            sparseWorld_->size());
}

std::vector<Actor*> GameMapBase::getCollisionAt(int x, int y,
                                                Actor& selfActor) {
    if (!selfActor.getCollisionEnabled() || !selfActor.isVisibleInHierarchy()) {
        return {};
    }
    const std::vector<Actor*>* actorsAtCell =
        occupancy_->findActorsAtCell(x, y, sparseWorld_->size());
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
        if (otherActor->isDestroyed() || !otherActor->isVisibleInHierarchy()) {
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
    if (!selfActor.isVisibleInHierarchy()) {
        return {};
    }
    const std::vector<Actor*>* actorsAtCell =
        occupancy_->findActorsAtCell(x, y, sparseWorld_->size());
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
        if (otherActor->isDestroyed() || !otherActor->isVisibleInHierarchy()) {
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
    const std::vector<Actor*>* actorsAtCell =
        occupancy_->findActorsAtCell(x, y, sparseWorld_->size());
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
                if (actor->isDestroyed() || !actor->isVisibleInHierarchy()) {
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
    if (sparseWorld_->size().has_value()) {
        return sparseWorld_->isSparseWorldTilePassable(position);
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
    auto layerIt =
        actorRegistry_->actorLayers().find(const_cast<Actor*>(actor));
    if (layerIt == actorRegistry_->actorLayers().end()) {
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
        if (actor->isDestroyed() || !actor->isVisibleInHierarchy()) {
            continue;
        }
        const int layerIndex = getActorLayerIndex(actor);
        if (layerIndex < topmostLayerIndex) {
            topmostLayerIndex = layerIndex;
        }
    }
    return topmostLayerIndex;
}

void GameMapBase::updateActorOccupancy(Actor& actor) {
    ensurePassabilityCache();
    occupancy_->unregisterActorOccupancy(actor, sparseWorld_->size());
    occupancy_->registerActorOccupancy(actor, sparseWorld_->size());
}

void GameMapBase::ensurePassabilityCache() const {
    if (sparseWorld_->size().has_value()) {
        if (!passabilityDirty_) {
            return;
        }
        GameMapBase* self = const_cast<GameMapBase*>(this);
        self->rebuildPassabilityCache(*sparseWorld_->size());
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
    for (const auto& [_, actorList] : actorRegistry_->actors()) {
        for (const ActorPtr& actor : actorList) {
            if (!actor || !seenActors.insert(actor.get()).second) {
                continue;
            }
            const std::vector<sf::Vector2i>* registered =
                occupancy_->registeredCells(actor.get());
            if (actor->isDestroyed()) {
                if (registered != nullptr) {
                    occupancy_->unregisterActorOccupancy(*actor,
                                                         sparseWorld_->size());
                }
                continue;
            }
            const std::vector<sf::Vector2i> occupiedCells =
                actor->getOccupiedMapCells();
            if (registered != nullptr && *registered == occupiedCells) {
                continue;
            }
            if (registered != nullptr) {
                occupancy_->unregisterActorOccupancy(*actor,
                                                     sparseWorld_->size());
            }
            occupancy_->registerActorOccupancy(*actor, sparseWorld_->size());
        }
    }
}

std::vector<std::string> GameMapBase::getTopFirstLayerNames() const {
    if (sparseWorld_->size().has_value()) {
        std::vector<std::string> layerNames = sparseWorld_->layerOrder();
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
    if (sparseWorld_->size().has_value()) {
        const std::optional<Material> material =
            sparseWorld_->getSparseWorldTopMaterial(pos, *actorRegistry_);
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

void GameMapBase::setHideDisconnectedRegions(bool enabled) {
    if (enabled && sparseWorld_->size().has_value()) {
        throw std::invalid_argument(
            "Disconnected-region hiding requires an ordinary tile map");
    }
    if (hideDisconnectedRegions_ != enabled) {
        hideDisconnectedRegions_ = enabled;
        visibilityDirty_ = true;
        ++visibilityRevision_;
    }
}

bool GameMapBase::getHideDisconnectedRegions() const {
    return hideDisconnectedRegions_;
}

void GameMapBase::setVisibilityObserver(std::optional<sf::Vector2i> position) {
    visibilityObserver_ = position;
}

void GameMapBase::ensureVisibilityCache() const {
    std::vector<VisibilityLayerState> layers;
    if (tilemap_) {
        for (const std::string& name : tilemap_->getLayerNameList()) {
            const auto layer = tilemap_->getLayer(name);
            layers.push_back({layer, layer && layer->getVisible(),
                              layer ? layer->getContentRevision() : 0});
        }
    }
    if (visibilityDirty_ || layers != visibilityLayers_) {
        if (hideDisconnectedRegions_) {
            auto* self = const_cast<GameMapBase*>(this);
            self->rebuildPassabilityCache(getSize());
            regionVisibility_->rebuild(tilePassableGrid_);
        }
        visibilityLayers_ = std::move(layers);
        visibilityDirty_ = false;
        ++visibilityRevision_;
    }
    if (!hideDisconnectedRegions_) {
        return;
    }
    auto observer = visibilityObserver_;
    if (!observer) {
        const auto& player = getPlayerActorForRenderer();
        if (player && !player->isDestroyed()) {
            observer = player->getMapPosition();
        }
    }
    if (regionVisibility_->setObserver(observer)) {
        ++visibilityRevision_;
    }
}

bool GameMapBase::isCellVisible(const sf::Vector2i& position) const {
    if (!hideDisconnectedRegions_) {
        return true;
    }
    ensureVisibilityCache();
    return regionVisibility_->isCellVisible(position);
}

bool GameMapBase::isActorVisibleOnMap(const Actor& actor) const {
    if (actor.isDestroyed() || !actor.isVisibleInHierarchy()) {
        return false;
    }
    if (!hideDisconnectedRegions_) {
        return true;
    }
    ensureVisibilityCache();
    std::shared_ptr<Actor> parent;
    for (const Actor* current = &actor; current != nullptr;
         current = parent.get()) {
        if (!regionVisibility_->isCellVisible(current->getMapPosition())) {
            return false;
        }
        parent = current->getParent();
    }
    return true;
}

std::size_t GameMapBase::getVisibilityRevision() const {
    ensureVisibilityCache();
    return visibilityRevision_;
}

std::vector<std::vector<sf::Vector2i>> GameMapBase::getDisplayTileSources()
    const {
    ensureVisibilityCache();
    const sf::Vector2u size = getSize();
    std::vector<std::vector<sf::Vector2i>> sources(
        size.y, std::vector<sf::Vector2i>(size.x));
    for (unsigned int y = 0; y < size.y; ++y) {
        for (unsigned int x = 0; x < size.x; ++x) {
            const sf::Vector2i position{static_cast<int>(x),
                                        static_cast<int>(y)};
            sources[y][x] =
                hideDisconnectedRegions_ &&
                        !regionVisibility_->isCellVisible(position)
                    ? regionVisibility_->replacementSource(position).value_or(
                          sf::Vector2i{-1, -1})
                    : position;
        }
    }
    return sources;
}

std::shared_ptr<sf::Texture> GameMapBase::rebuildRenderLightOccupancy(
    const std::shared_ptr<Tilemap>& displayTilemap,
    const std::vector<std::shared_ptr<Actor>>& actors) {
    return lightOcclusion_->rebuildStaticLightOccupancy(
        {0, 0}, displayTilemap->getSize(), actors, *sparseWorld_,
        displayTilemap, EngineState::CellSize);
}
