#include "GameMapBase.hpp"
#include "GameMapBase/ActorRegistry.hpp"
#include "GameMapBase/LightOcclusionRuntime.hpp"
#include "GameMapBase/OccupancyRuntime.hpp"
#include "GameMapBase/PathfindingRuntime.hpp"
#include "GameMapBase/SparseWorldRuntime.hpp"

#include <Gameplay/Actor.hpp>
#include <Gameplay/TileMap.hpp>
#include <EngineState.hpp>

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

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

using ludork::global::game_map_impl::CellBounds;
using ludork::global::game_map_impl::cellBounds;
using ludork::global::game_map_impl::intersection;
using ludork::global::game_map_impl::isEmpty;

GameMapBase::GameMapBase()
    : actorRegistry_(std::make_unique<GameMapActorRegistry>()) {}

GameMapBase::~GameMapBase() = default;

const ActorDict& GameMapBase::getMaterialActorsForRenderer() const {
    return materialActorsRef_;
}

const ActorPtr& GameMapBase::getPlayerActorForRenderer() const {
    return playerActor_;
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

namespace {

class BoolResetGuard {
public:
    explicit BoolResetGuard(bool& value) : value_(value) {
        value_ = true;
    }

    BoolResetGuard(const BoolResetGuard&) = delete;
    BoolResetGuard& operator=(const BoolResetGuard&) = delete;

    ~BoolResetGuard() {
        value_ = false;
    }

private:
    bool& value_;
};

class DepthGuard {
public:
    explicit DepthGuard(std::size_t& depth) : depth_(depth) {
        ++depth_;
    }

    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;

    ~DepthGuard() {
        --depth_;
    }

private:
    std::size_t& depth_;
};

void rememberLayer(GameMapActorRegistryEntry& entry, const std::string& layer) {
    if (std::find(entry.layerOrder.begin(), entry.layerOrder.end(), layer) ==
        entry.layerOrder.end()) {
        entry.layerOrder.push_back(layer);
    }
}

}  // namespace

void GameMapBase::syncActorsRef(const ActorDict& actors) {
    clearActorOccupancy();
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

bool GameMapBase::registerLayerActor(ActorPtr actor, const std::string& layer) {
    if (!actor) {
        return false;
    }
    Actor* actorPointer = actor.get();
    auto& membership = actorRegistry_->layerMembership[layer];
    if (!membership.insert(actorPointer).second) {
        return false;
    }

    GameMapActorRegistryEntry& entry = actorRegistry_->entries[actorPointer];
    entry.owner = std::move(actor);
    rememberLayer(entry, layer);
    entry.liveLayers.insert(layer);
    for (const std::string& registeredLayer : entry.layerOrder) {
        if (entry.liveLayers.contains(registeredLayer)) {
            actorLayerRef_[actorPointer] = registeredLayer;
            break;
        }
    }
    actorRegistry_->pendingCreateActors.push_back(actorPointer);
    actorRegistry_->pendingComponentActors.push_back(actorPointer);
    actorRegistry_->viewsDirty = true;
    return true;
}

std::optional<std::string> GameMapBase::getRegisteredActorLayer(
    Actor& actor) const {
    const auto layerIt = actorLayerRef_.find(&actor);
    if (layerIt == actorLayerRef_.end()) {
        return std::nullopt;
    }
    return layerIt->second;
}

void GameMapBase::beginActorBatch() {
    ++actorRegistry_->batchDepth;
}

void GameMapBase::endActorBatch() {
    if (actorRegistry_->batchDepth == 0) {
        throw std::logic_error("Actor batch is not active");
    }
    --actorRegistry_->batchDepth;
    if (actorRegistry_->batchDepth == 0) {
        flushActorChanges();
    }
}

void GameMapBase::flushActorChanges() {
    if (!actorRegistry_->viewsDirty || actorRegistry_->batchDepth != 0 ||
        actorRegistry_->viewSyncDeferDepth != 0 ||
        actorRegistry_->initialising || actorRegistry_->flushing ||
        !actorListUpdater_) {
        return;
    }
    BoolResetGuard flushingGuard(actorRegistry_->flushing);
    actorListUpdater_();
}

void GameMapBase::withDeferredActorViewSync(std::function<void()> handler) {
    actorRegistry_->viewsDirty = true;
    passabilityDirty_ = true;
    {
        DepthGuard deferGuard(actorRegistry_->viewSyncDeferDepth);
        handler();
    }
    flushActorChanges();
}

void GameMapBase::drainActorLifecycle(
    std::function<void(Actor&)> createHandler,
    std::function<void(Actor&)> componentHandler) {
    if (actorRegistry_->initialising || actorRegistry_->batchDepth != 0) {
        return;
    }
    {
        BoolResetGuard initialisingGuard(actorRegistry_->initialising);
        while (true) {
            bool createdAny = false;
            while (!actorRegistry_->pendingCreateActors.empty()) {
                std::vector<Actor*> pendingActors =
                    std::move(actorRegistry_->pendingCreateActors);
                actorRegistry_->pendingCreateActors.clear();
                std::vector<Actor*> liveActors;
                liveActors.reserve(pendingActors.size());
                for (Actor* actor : pendingActors) {
                    const auto entryIt = actorRegistry_->entries.find(actor);
                    if (entryIt == actorRegistry_->entries.end() ||
                        entryIt->second.createInitialised ||
                        actor->isDestroyed() ||
                        !actorLayerRef_.contains(actor)) {
                        continue;
                    }
                    liveActors.push_back(actor);
                }
                for (Actor* actor : liveActors) {
                    GameMapActorRegistryEntry& entry =
                        actorRegistry_->entries.at(actor);
                    if (entry.createInitialised) {
                        continue;
                    }
                    entry.createInitialised = true;
                    createHandler(*actor);
                    createdAny = true;
                }
            }

            bool componentAny = false;
            std::vector<Actor*> pendingActors =
                std::move(actorRegistry_->pendingComponentActors);
            actorRegistry_->pendingComponentActors.clear();
            std::vector<Actor*> liveActors;
            liveActors.reserve(pendingActors.size());
            for (Actor* actor : pendingActors) {
                const auto entryIt = actorRegistry_->entries.find(actor);
                if (entryIt == actorRegistry_->entries.end() ||
                    entryIt->second.componentInitialised ||
                    actor->isDestroyed() || !actorLayerRef_.contains(actor)) {
                    continue;
                }
                liveActors.push_back(actor);
            }
            for (Actor* actor : liveActors) {
                GameMapActorRegistryEntry& entry =
                    actorRegistry_->entries.at(actor);
                if (entry.componentInitialised) {
                    continue;
                }
                entry.componentInitialised = true;
                componentHandler(*actor);
                componentAny = true;
            }

            if (!createdAny && !componentAny) {
                break;
            }
        }
    }
    flushActorChanges();
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
    ActorDict nextActorsRef;
    ActorDict nextMaterialActorsRef;
    std::unordered_map<std::string, std::unordered_set<Actor*>>
        nextLayerMembership;
    std::unordered_map<Actor*, std::string> firstExpandedLayer;
    std::vector<ActorPtr> updateActors;

    for (const auto& [layerName, actorList] : actors) {
        std::vector<ActorPtr>& expandedActors = nextActorsRef[layerName];
        std::vector<ActorPtr>& materialActors =
            nextMaterialActorsRef[layerName];
        std::unordered_set<Actor*>& membership = nextLayerMembership[layerName];
        std::vector<ActorPtr> queue;
        queue.reserve(actorList.size());
        materialActors.reserve(actorList.size());
        membership.reserve(actorList.size());

        for (const ActorPtr& actor : actorList) {
            if (!actor) {
                continue;
            }
            Actor* actorPointer = actor.get();
            membership.insert(actorPointer);
            materialActors.push_back(actor);
            updateActors.push_back(actor);
            queue.push_back(actor);
            GameMapActorRegistryEntry& entry =
                actorRegistry_->entries[actorPointer];
            entry.owner = actor;
            rememberLayer(entry, layerName);
        }

        for (std::size_t index = 0; index < queue.size(); ++index) {
            const ActorPtr actor = queue[index];
            if (!actor) {
                continue;
            }
            Actor* actorPointer = actor.get();
            actor->setMap(mapOwner);
            expandedActors.push_back(actor);
            firstExpandedLayer.try_emplace(actorPointer, layerName);
            GameMapActorRegistryEntry& entry =
                actorRegistry_->entries[actorPointer];
            entry.owner = actor;
            for (const ActorPtr& child : actor->getChildren()) {
                if (child) {
                    queue.push_back(child);
                }
            }
        }
    }

    actorRegistry_->updateBatch.syncActors(updateActors);

    for (auto& [_, entry] : actorRegistry_->entries) {
        entry.liveLayers.clear();
    }
    for (const auto& [layerName, membership] : nextLayerMembership) {
        for (Actor* actor : membership) {
            GameMapActorRegistryEntry& entry =
                actorRegistry_->entries.at(actor);
            rememberLayer(entry, layerName);
            entry.liveLayers.insert(layerName);
        }
    }

    std::unordered_map<Actor*, std::string> nextActorLayerRef;
    nextActorLayerRef.reserve(firstExpandedLayer.size());
    for (const auto& [actor, entry] : actorRegistry_->entries) {
        for (const std::string& layerName : entry.layerOrder) {
            if (entry.liveLayers.contains(layerName)) {
                nextActorLayerRef.emplace(actor, layerName);
                break;
            }
        }
    }
    for (const auto& [actor, layerName] : firstExpandedLayer) {
        nextActorLayerRef.try_emplace(actor, layerName);
    }

    std::unordered_set<Actor*> destroyedEntries;
    for (auto iterator = actorRegistry_->entries.begin();
         iterator != actorRegistry_->entries.end();) {
        Actor* actor = iterator->first;
        if (actor != playerActor_.get() && !nextActorLayerRef.contains(actor) &&
            iterator->second.owner && iterator->second.owner->isDestroyed()) {
            destroyedEntries.insert(actor);
            iterator = actorRegistry_->entries.erase(iterator);
        } else {
            ++iterator;
        }
    }
    if (!destroyedEntries.empty()) {
        std::erase_if(actorRegistry_->pendingCreateActors, [&](Actor* actor) {
            return destroyedEntries.contains(actor);
        });
        std::erase_if(actorRegistry_->pendingComponentActors,
                      [&](Actor* actor) {
                          return destroyedEntries.contains(actor);
                      });
    }

    clearActorOccupancy();
    registeredOccupancyCells_.clear();
    actorsRef_ = std::move(nextActorsRef);
    materialActorsRef_ = std::move(nextMaterialActorsRef);
    actorLayerRef_ = std::move(nextActorLayerRef);
    actorRegistry_->layerMembership = std::move(nextLayerMembership);
    passabilityDirty_ = true;
    actorRegistry_->viewsDirty = false;
}

void GameMapBase::forgetActors(const std::vector<ActorPtr>& actors) {
    std::unordered_set<Actor*> targets;
    targets.reserve(actors.size());
    for (const ActorPtr& actor : actors) {
        if (actor) {
            targets.insert(actor.get());
        }
    }
    if (targets.empty()) {
        return;
    }

    const auto containsTarget = [&](const ActorDict& actorDict) {
        for (const auto& [_, actorList] : actorDict) {
            for (const ActorPtr& actor : actorList) {
                if (actor && targets.contains(actor.get())) {
                    return true;
                }
            }
        }
        return false;
    };
    if ((playerActor_ && targets.contains(playerActor_.get())) ||
        containsTarget(actorsRef_) || containsTarget(materialActorsRef_)) {
        throw std::logic_error("Cannot forget a live or player Actor");
    }
    for (Actor* actor : targets) {
        if (actorLayerRef_.contains(actor)) {
            throw std::logic_error("Cannot forget a live or player Actor");
        }
    }

    for (Actor* actor : targets) {
        if (registeredOccupancyCells_.contains(actor)) {
            unregisterActorOccupancy(*actor);
        }
        actorLayerRef_.erase(actor);
        actorRegistry_->entries.erase(actor);
    }
    for (auto iterator = actorRegistry_->layerMembership.begin();
         iterator != actorRegistry_->layerMembership.end();) {
        for (Actor* actor : targets) {
            iterator->second.erase(actor);
        }
        if (iterator->second.empty()) {
            iterator = actorRegistry_->layerMembership.erase(iterator);
        } else {
            ++iterator;
        }
    }
    std::erase_if(actorRegistry_->pendingCreateActors, [&](Actor* actor) {
        return targets.contains(actor);
    });
    std::erase_if(actorRegistry_->pendingComponentActors, [&](Actor* actor) {
        return targets.contains(actor);
    });

    std::vector<ActorPtr> updateActors;
    for (const auto& [_, actorList] : materialActorsRef_) {
        updateActors.insert(updateActors.end(), actorList.begin(),
                            actorList.end());
    }
    actorRegistry_->updateBatch.syncActors(updateActors);
    passabilityDirty_ = true;
}

void GameMapBase::updateActors(float deltaTime) {
    actorRegistry_->updateBatch.update(deltaTime);
}

void GameMapBase::lateUpdateActors(float deltaTime) {
    actorRegistry_->updateBatch.lateUpdate(deltaTime);
}

void GameMapBase::fixedUpdateActors(float fixedDelta) {
    actorRegistry_->updateBatch.fixedUpdate(fixedDelta);
}

void GameMapBase::updateActorList() {
    actorRegistry_->viewsDirty = true;
    passabilityDirty_ = true;
    flushActorChanges();
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

void GameMapBase::clearStaticLightOccupancy() {
    staticLightOccupancy_.reset();
    staticLightOccupancyOrigin_ = {};
    staticLightOccupancySize_ = {};
    staticLightOccupancyPrefix_.clear();
}

std::shared_ptr<sf::Texture> GameMapBase::rebuildStaticLightOccupancy(
    const sf::Vector2i& origin, const sf::Vector2u& size,
    const std::vector<std::shared_ptr<Actor>>& actors) {
    if (size.x == 0 || size.y == 0) {
        throw std::invalid_argument(
            "Static light occupancy size must be positive");
    }
    const std::size_t width = size.x;
    const std::size_t height = size.y;
    if (height > std::numeric_limits<std::size_t>::max() / width) {
        throw std::length_error("Static light occupancy size is too large");
    }
    const std::size_t cellCount = width * height;
    if (cellCount > std::numeric_limits<std::size_t>::max() / 4) {
        throw std::length_error("Static light occupancy image is too large");
    }

    std::vector<std::uint8_t> occupancy(cellCount, 0);
    const CellBounds targetBounds = cellBounds(origin, size);
    const auto markLayer = [&](TileLayer& layer,
                               const sf::Vector2i& layerOrigin) {
        if (!layer.getVisible()) {
            return;
        }
        const sf::Vector2u layerSize = layer.getGridSize();
        const CellBounds overlap =
            intersection(targetBounds, cellBounds(layerOrigin, layerSize));
        if (isEmpty(overlap)) {
            return;
        }
        const std::vector<std::vector<float>>& lightBlockMap =
            layer.getLightBlockMapView();
        for (std::int64_t worldY = overlap.top; worldY < overlap.bottom;
             ++worldY) {
            const std::size_t localY = static_cast<std::size_t>(
                worldY - static_cast<std::int64_t>(layerOrigin.y));
            const std::size_t occupancyY = static_cast<std::size_t>(
                worldY - static_cast<std::int64_t>(origin.y));
            const std::vector<float>& lightBlockRow = lightBlockMap[localY];
            for (std::int64_t worldX = overlap.left; worldX < overlap.right;
                 ++worldX) {
                const std::size_t localX = static_cast<std::size_t>(
                    worldX - static_cast<std::int64_t>(layerOrigin.x));
                if (lightBlockRow[localX] <= 0.0f) {
                    continue;
                }
                const std::size_t occupancyX = static_cast<std::size_t>(
                    worldX - static_cast<std::int64_t>(origin.x));
                occupancy[occupancyY * width + occupancyX] = 1;
            }
        }
    };

    if (sparseWorldSize_.has_value()) {
        for (SparseWorldRegion& region : sparseWorldRegions_) {
            if (!region.tilemap) {
                continue;
            }
            const CellBounds overlap =
                intersection(targetBounds, cellBounds(region.rect));
            if (isEmpty(overlap)) {
                continue;
            }
            for (const std::shared_ptr<TileLayer>& layer :
                 region.layersTopFirst) {
                markLayer(*layer, region.rect.position);
            }
        }
    } else if (tilemap_) {
        for (const std::string& layerName : tilemap_->getLayerNameList()) {
            const std::shared_ptr<TileLayer> layer =
                tilemap_->getLayer(layerName);
            if (layer) {
                markLayer(*layer, {});
            }
        }
    }
    const double cellSize = static_cast<double>(CellSize);
    for (const std::shared_ptr<Actor>& actor : actors) {
        if (!actor || actor->isDestroyed() || actor->getLightBlock() <= 0.0f) {
            continue;
        }
        const sf::FloatRect bounds = actor->getGlobalBounds();
        const CellBounds actorCells{
            static_cast<std::int64_t>(
                std::floor(static_cast<double>(bounds.position.x) / cellSize)),
            static_cast<std::int64_t>(
                std::floor(static_cast<double>(bounds.position.y) / cellSize)),
            static_cast<std::int64_t>(std::ceil(
                static_cast<double>(bounds.position.x + bounds.size.x) /
                cellSize)),
            static_cast<std::int64_t>(std::ceil(
                static_cast<double>(bounds.position.y + bounds.size.y) /
                cellSize))};
        const CellBounds overlap = intersection(targetBounds, actorCells);
        for (std::int64_t worldY = overlap.top; worldY < overlap.bottom;
             ++worldY) {
            const std::size_t occupancyY = static_cast<std::size_t>(
                worldY - static_cast<std::int64_t>(origin.y));
            for (std::int64_t worldX = overlap.left; worldX < overlap.right;
                 ++worldX) {
                const std::size_t occupancyX = static_cast<std::size_t>(
                    worldX - static_cast<std::int64_t>(origin.x));
                occupancy[occupancyY * width + occupancyX] = 1;
            }
        }
    }

    const std::size_t prefixWidth = width + 1;
    if (height + 1 > std::numeric_limits<std::size_t>::max() / prefixWidth) {
        throw std::length_error("Static light occupancy prefix is too large");
    }
    std::vector<std::size_t> prefix((height + 1) * prefixWidth, 0);
    std::vector<std::uint8_t> pixels(cellCount * 4, 0);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const std::size_t value = occupancy[y * width + x];
            const std::size_t prefixIndex = (y + 1) * prefixWidth + x + 1;
            prefix[prefixIndex] = value + prefix[prefixIndex - 1] +
                                  prefix[prefixIndex - prefixWidth] -
                                  prefix[prefixIndex - prefixWidth - 1];

            const std::size_t textureY = height - y - 1;
            const std::size_t pixelIndex = (textureY * width + x) * 4;
            const std::uint8_t channel = value == 0 ? 0 : 255;
            pixels[pixelIndex] = channel;
            pixels[pixelIndex + 1] = channel;
            pixels[pixelIndex + 2] = channel;
            pixels[pixelIndex + 3] = 255;
        }
    }

    sf::Image image(size, pixels.data());
    std::shared_ptr<sf::Texture> texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromImage(image)) {
        throw std::runtime_error(
            "Failed to create the static light occupancy texture");
    }
    texture->setSmooth(false);

    staticLightOccupancy_ = texture;
    staticLightOccupancyOrigin_ = origin;
    staticLightOccupancySize_ = size;
    staticLightOccupancyPrefix_ = std::move(prefix);
    return texture;
}

bool GameMapBase::hasStaticLightOccupancy(const Light& light) const {
    return ludork::global::game_map_impl::hasStaticOccupancy(
        staticLightOccupancyPrefix_, staticLightOccupancyOrigin_,
        staticLightOccupancySize_, light, CellSize);
}

std::vector<LightOcclusionResult> GameMapBase::analyseLightOcclusion(
    const std::vector<LightOcclusionInput>& inputs,
    const std::vector<std::shared_ptr<Actor>>& visibleActors) {
    ensurePassabilityCache();
    std::unordered_map<Actor*, std::shared_ptr<Actor>> visibleActorOwners;
    visibleActorOwners.reserve(visibleActors.size());
    for (const std::shared_ptr<Actor>& actor : visibleActors) {
        if (actor) {
            visibleActorOwners.try_emplace(actor.get(), actor);
        }
    }

    std::vector<LightOcclusionResult> results;
    results.reserve(inputs.size());
    dynamicLightOccupancies_.resize(inputs.size());
    for (std::size_t inputIndex = 0; inputIndex < inputs.size(); ++inputIndex) {
        const LightOcclusionInput& input = inputs[inputIndex];
        LightOcclusionResult result;
        result.hasStaticTransmissionLoss = hasStaticLightOccupancy(input.light);
        const Light& light = input.light;
        if (light.radius <= 0.0f || !std::isfinite(light.position.x) ||
            !std::isfinite(light.position.y) || !std::isfinite(light.radius)) {
            results.push_back(std::move(result));
            continue;
        }

        const float cellSize = static_cast<float>(CellSize);
        const float rawCellRadius =
            std::ceil((light.radius + cellSize) / cellSize);
        const float rawMapX = std::floor(light.position.x / cellSize);
        const float rawMapY = std::floor(light.position.y / cellSize);
        if (rawCellRadius >
                static_cast<float>(std::numeric_limits<int>::max()) ||
            rawMapX < static_cast<float>(std::numeric_limits<int>::min()) ||
            rawMapX > static_cast<float>(std::numeric_limits<int>::max()) ||
            rawMapY < static_cast<float>(std::numeric_limits<int>::min()) ||
            rawMapY > static_cast<float>(std::numeric_limits<int>::max())) {
            results.push_back(std::move(result));
            continue;
        }
        const int cellRadius = static_cast<int>(rawCellRadius);
        const int mapX = static_cast<int>(rawMapX);
        const int mapY = static_cast<int>(rawMapY);
        const std::vector<Actor*> actors =
            getActorsInRangeImpl(mapX, mapY, cellRadius, input.owner.get());

        std::optional<sf::FloatRect> actorBounds;
        std::vector<sf::FloatRect> occluderBounds;
        for (Actor* actor : actors) {
            const auto visibleActor = visibleActorOwners.find(actor);
            if (visibleActor == visibleActorOwners.end() ||
                actor->isDestroyed() || actor->getLightBlock() <= 0.0f) {
                continue;
            }
            const sf::FloatRect bounds = actor->getGlobalBounds();
            if (!ludork::global::game_map_impl::actorIntersectsLight(bounds,
                                                                     light)) {
                continue;
            }
            result.occluders.push_back(visibleActor->second);
            occluderBounds.push_back(bounds);
            if (!actorBounds.has_value()) {
                actorBounds = bounds;
                continue;
            }
            actorBounds = ludork::global::game_map_impl::enclosingRect(
                *actorBounds, bounds);
        }

        result.maskRect = ludork::global::game_map_impl::dynamicMaskRect(
            light, actorBounds, 2.0f);
        if (result.maskRect.has_value()) {
            ludork::global::game_map_impl::DynamicOccupancyResult occupancy =
                ludork::global::game_map_impl::rebuildDynamicOccupancy(
                    *result.maskRect, occluderBounds,
                    dynamicLightOccupancies_[inputIndex], cellSize);
            dynamicLightOccupancies_[inputIndex] = occupancy.texture;
            result.dynamicOccupancy = std::move(occupancy.texture);
            result.dynamicOccupancyOrigin = occupancy.origin;
            result.dynamicOccupancySize = occupancy.size;
        }
        results.push_back(std::move(result));
    }
    return results;
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
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    std::vector<IntPair> excludedAnchorSet;
    excludedAnchorSet.reserve(excludedAnchors.size());
    for (const sf::Vector2i& anchor : excludedAnchors) {
        excludedAnchorSet.emplace_back(anchor.x, anchor.y);
    }
    PathResult result;
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
    std::unordered_set<IntPair, IntPairHash> fromCellSet;
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
    if (sparseWorldSize_.has_value()) {
        return isSparseWorldDirectionPassable(fromPosition, toPosition,
                                              direction);
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

bool GameMapBase::isSparseWorldDirectionPassable(
    const sf::Vector2i& fromPosition, const sf::Vector2i& toPosition,
    int direction) const {
    if (!isSparseWorldGameplayPositionReady(fromPosition) ||
        !isSparseWorldGameplayPositionReady(toPosition)) {
        return false;
    }
    const int opposite = oppositeDirection(direction);
    const SparseWorldRegion* fromRegion = findSparseWorldRegion(fromPosition);
    const SparseWorldRegion* toRegion = findSparseWorldRegion(toPosition);
    const sf::Vector2i fromLocal =
        fromRegion == nullptr ? sf::Vector2i{}
                              : fromPosition - fromRegion->rect.position;
    const sf::Vector2i toLocal = toRegion == nullptr
                                     ? sf::Vector2i{}
                                     : toPosition - toRegion->rect.position;
    bool fromFound = false;
    bool toFound = false;
    for (std::size_t index = 0; index < sparseWorldLayerOrder_.size();
         ++index) {
        if (!fromFound && fromRegion != nullptr) {
            const std::shared_ptr<TileLayer>& layer =
                fromRegion->layersTopFirst[index];
            if (layer->getVisible() && layer->hasContent(fromLocal)) {
                if (!layer->isDirectionPassable(fromLocal, direction)) {
                    return false;
                }
                fromFound = true;
            }
        }
        if (!toFound && toRegion != nullptr) {
            const std::shared_ptr<TileLayer>& layer =
                toRegion->layersTopFirst[index];
            if (layer->getVisible() && layer->hasContent(toLocal)) {
                if (!layer->isDirectionPassable(toLocal, opposite)) {
                    return false;
                }
                toFound = true;
            }
        }
        if (fromFound && toFound) {
            break;
        }
    }
    return true;
}

using ludork::global::game_map_base_impl::rectContains;
using ludork::global::game_map_base_impl::rectsIntersect;

std::size_t IntPairHash::operator()(const IntPair& value) const {
    std::size_t xHash = std::hash<int>{}(value.first);
    std::size_t yHash = std::hash<int>{}(value.second);
    return xHash ^ (yHash + 0x9e3779b9 + (xHash << 6) + (xHash >> 2));
}

void GameMapBase::configureSparseWorld(
    const sf::Vector2u& size, const std::vector<std::string>& layerOrder,
    const std::vector<sf::IntRect>& regionRects) {
    if (size.x > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        size.y > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Sparse world size is out of range");
    }

    std::unordered_set<std::string> uniqueLayerNames;
    uniqueLayerNames.reserve(layerOrder.size());
    for (const std::string& layerName : layerOrder) {
        if (!uniqueLayerNames.insert(layerName).second) {
            throw std::invalid_argument(
                "Sparse world layer order contains a duplicate name");
        }
    }

    std::vector<SparseWorldRegion> regions;
    regions.reserve(regionRects.size());
    SparseWorldRegionPageMap regionPages;
    for (const sf::IntRect& rect : regionRects) {
        const std::int64_t right =
            static_cast<std::int64_t>(rect.position.x) + rect.size.x;
        const std::int64_t bottom =
            static_cast<std::int64_t>(rect.position.y) + rect.size.y;
        if (rect.position.x < 0 || rect.position.y < 0 || rect.size.x <= 0 ||
            rect.size.y <= 0 || right > static_cast<std::int64_t>(size.x) ||
            bottom > static_cast<std::int64_t>(size.y)) {
            throw std::invalid_argument(
                "Sparse world region rectangle is outside the world");
        }
        for (const SparseWorldRegion& existing : regions) {
            if (rectsIntersect(existing.rect, rect)) {
                throw std::invalid_argument(
                    "Sparse world region rectangles must not overlap");
            }
        }
        regions.push_back({.rect = rect});
        const std::size_t regionIndex = regions.size() - 1;
        const IntPair firstPage =
            getOccupancyPageKey(rect.position.x, rect.position.y);
        const IntPair lastPage = getOccupancyPageKey(
            static_cast<int>(right - 1), static_cast<int>(bottom - 1));
        for (int pageY = firstPage.second; pageY <= lastPage.second; ++pageY) {
            for (int pageX = firstPage.first; pageX <= lastPage.first;
                 ++pageX) {
                regionPages[{pageX, pageY}].push_back(regionIndex);
            }
        }
    }

    sparseWorldSize_ = size;
    sparseWorldLayerOrder_ = layerOrder;
    sparseWorldRegions_ = std::move(regions);
    sparseWorldRegionPages_ = std::move(regionPages);
    sparseWorldPreparedRect_.reset();
    clearStaticLightOccupancy();
    clearActorOccupancy();
    registeredOccupancyCells_.clear();
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

void GameMapBase::setSparseWorldRegion(int regionIndex,
                                       std::shared_ptr<Tilemap> tilemap,
                                       bool actorsReady) {
    if (!tilemap) {
        throw std::invalid_argument("Sparse world region Tilemap is null");
    }
    SparseWorldRegion& region = requireSparseWorldRegion(regionIndex);
    const sf::Vector2u expectedSize(
        static_cast<unsigned int>(region.rect.size.x),
        static_cast<unsigned int>(region.rect.size.y));
    if (tilemap->getSize() != expectedSize) {
        throw std::invalid_argument(
            "Sparse world region Tilemap size does not match its rectangle");
    }

    std::vector<std::shared_ptr<TileLayer>> layersTopFirst;
    layersTopFirst.reserve(sparseWorldLayerOrder_.size());
    for (auto layerName = sparseWorldLayerOrder_.rbegin();
         layerName != sparseWorldLayerOrder_.rend(); ++layerName) {
        std::shared_ptr<TileLayer> layer = tilemap->getLayer(*layerName);
        if (!layer) {
            throw std::invalid_argument(
                "Sparse world region Tilemap is missing a configured layer");
        }
        if (layer->getGridSize() != expectedSize) {
            throw std::invalid_argument(
                "Sparse world region layer size does not match its rectangle");
        }
        layersTopFirst.push_back(std::move(layer));
    }

    region.tilemap = std::move(tilemap);
    region.layersTopFirst = std::move(layersTopFirst);
    region.actorsReady = actorsReady;
    clearStaticLightOccupancy();
}

void GameMapBase::setSparseWorldRegionActorsReady(int regionIndex) {
    SparseWorldRegion& region = requireSparseWorldRegion(regionIndex);
    if (!region.tilemap) {
        throw std::logic_error(
            "Sparse world region Tilemap must be set before Actors are ready");
    }
    region.actorsReady = true;
}

void GameMapBase::detachSparseWorldRegion(int regionIndex) {
    SparseWorldRegion& region = requireSparseWorldRegion(regionIndex);
    if (!region.tilemap && region.layersTopFirst.empty() &&
        !region.actorsReady) {
        return;
    }
    region.tilemap.reset();
    region.layersTopFirst.clear();
    region.actorsReady = false;
    clearStaticLightOccupancy();
}

void GameMapBase::setSparseWorldPreparedRect(std::optional<sf::IntRect> rect) {
    if (!rect.has_value()) {
        sparseWorldPreparedRect_.reset();
        return;
    }
    if (!sparseWorldSize_.has_value()) {
        throw std::logic_error("Sparse world is not configured");
    }
    const std::int64_t right =
        static_cast<std::int64_t>(rect->position.x) + rect->size.x;
    const std::int64_t bottom =
        static_cast<std::int64_t>(rect->position.y) + rect->size.y;
    if (rect->position.x < 0 || rect->position.y < 0 || rect->size.x < 0 ||
        rect->size.y < 0 ||
        right > static_cast<std::int64_t>(sparseWorldSize_->x) ||
        bottom > static_cast<std::int64_t>(sparseWorldSize_->y)) {
        throw std::invalid_argument(
            "Sparse world prepared rectangle is outside the world");
    }
    sparseWorldPreparedRect_ = *rect;
}

bool GameMapBase::isSparseWorldCellReady(const sf::Vector2i& position) const {
    if (!sparseWorldSize_.has_value() || position.x < 0 || position.y < 0 ||
        position.x >= static_cast<int>(sparseWorldSize_->x) ||
        position.y >= static_cast<int>(sparseWorldSize_->y)) {
        return false;
    }
    const SparseWorldRegion* region = findSparseWorldRegion(position);
    if (region == nullptr) {
        return true;
    }
    if (!region->tilemap || !region->actorsReady) {
        return false;
    }
    const sf::Vector2i localPosition = position - region->rect.position;
    for (const std::shared_ptr<TileLayer>& layer : region->layersTopFirst) {
        if (!layer->isCellBuilt(localPosition)) {
            return false;
        }
    }
    return true;
}

bool GameMapBase::isSparseWorldGameplayPositionReady(
    const sf::Vector2i& position) const {
    return (!sparseWorldPreparedRect_.has_value() ||
            rectContains(*sparseWorldPreparedRect_, position)) &&
           isSparseWorldCellReady(position);
}

std::optional<int> GameMapBase::getSparseWorldRegionIndexAt(
    const sf::Vector2i& position) const {
    const auto pageIt = sparseWorldRegionPages_.find(
        getOccupancyPageKey(position.x, position.y));
    if (pageIt == sparseWorldRegionPages_.end()) {
        return std::nullopt;
    }
    for (const std::size_t regionIndex : pageIt->second) {
        const SparseWorldRegion& region = sparseWorldRegions_[regionIndex];
        if (rectContains(region.rect, position)) {
            return static_cast<int>(regionIndex + 1);
        }
    }
    return std::nullopt;
}

void GameMapBase::clearSparseWorld() {
    sparseWorldSize_.reset();
    sparseWorldLayerOrder_.clear();
    sparseWorldRegions_.clear();
    sparseWorldRegionPages_.clear();
    sparseWorldPreparedRect_.reset();
    clearStaticLightOccupancy();
    clearActorOccupancy();
    registeredOccupancyCells_.clear();
    tilePassableGrid_.clear();
    passabilityDirty_ = true;
}

const GameMapBase::SparseWorldRegion* GameMapBase::findSparseWorldRegion(
    const sf::Vector2i& position) const {
    const std::optional<int> regionIndex =
        getSparseWorldRegionIndexAt(position);
    if (!regionIndex.has_value()) {
        return nullptr;
    }
    return &sparseWorldRegions_[static_cast<std::size_t>(*regionIndex - 1)];
}

GameMapBase::SparseWorldRegion& GameMapBase::requireSparseWorldRegion(
    int regionIndex) {
    if (!sparseWorldSize_.has_value()) {
        throw std::logic_error("Sparse world is not configured");
    }
    if (regionIndex <= 0 ||
        regionIndex > static_cast<int>(sparseWorldRegions_.size())) {
        throw std::out_of_range("Sparse world region index is out of range");
    }
    return sparseWorldRegions_[static_cast<std::size_t>(regionIndex - 1)];
}

bool GameMapBase::isSparseWorldTilePassable(
    const sf::Vector2i& position) const {
    if (!isSparseWorldGameplayPositionReady(position)) {
        return false;
    }
    const SparseWorldRegion* region = findSparseWorldRegion(position);
    if (region == nullptr) {
        return true;
    }
    const sf::Vector2i localPosition = position - region->rect.position;
    for (const std::shared_ptr<TileLayer>& layer : region->layersTopFirst) {
        if (!layer->getVisible() || !layer->hasContent(localPosition)) {
            continue;
        }
        return layer->isPassable(localPosition);
    }
    return true;
}

std::optional<Material> GameMapBase::getSparseWorldTopMaterial(
    const sf::Vector2i& position) const {
    if (!isSparseWorldCellReady(position)) {
        return std::nullopt;
    }
    const SparseWorldRegion* region = findSparseWorldRegion(position);
    if (region == nullptr) {
        return std::nullopt;
    }
    const sf::Vector2i localPosition = position - region->rect.position;
    for (std::size_t index = 0; index < sparseWorldLayerOrder_.size();
         ++index) {
        const std::string& layerName =
            sparseWorldLayerOrder_[sparseWorldLayerOrder_.size() - index - 1];
        const auto actorLayerIt = materialActorsRef_.find(layerName);
        if (actorLayerIt != materialActorsRef_.end()) {
            for (const ActorPtr& actor : actorLayerIt->second) {
                if (actor && actor.get() != playerActor_.get() &&
                    !actor->isDestroyed() &&
                    actor->getMapPosition() == position) {
                    return actor->getMaterial();
                }
            }
        }
        const std::shared_ptr<TileLayer>& layer = region->layersTopFirst[index];
        if (!layer->getVisible()) {
            continue;
        }
        const std::optional<Material> material =
            layer->getMaterial(localPosition);
        if (material.has_value()) {
            return material;
        }
    }
    return std::nullopt;
}

std::size_t GameMapBase::getSparseOccupancyPageCount() const {
    return sparseOccupancyPages_.size();
}

int GameMapBase::getOccupancyPageCoordinate(int value) {
    return ludork::global::game_map_base_impl::pageCoordinate(
        value, OccupancyPageSize);
}

int GameMapBase::getOccupancyPageOffset(int value) {
    return ludork::global::game_map_base_impl::pageOffset(value,
                                                          OccupancyPageSize);
}

IntPair GameMapBase::getOccupancyPageKey(int x, int y) {
    return ludork::global::game_map_base_impl::pageKey(x, y, OccupancyPageSize);
}

std::size_t GameMapBase::getOccupancyPageCellIndex(int x, int y) {
    return ludork::global::game_map_base_impl::pageCellIndex(x, y,
                                                             OccupancyPageSize);
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
                ? isSparseWorldTilePassable(cell)
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
        return getSparseWorldTopMaterial(position);
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
        return isSparseWorldTilePassable(position);
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
        const std::optional<Material> material = getSparseWorldTopMaterial(pos);
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
