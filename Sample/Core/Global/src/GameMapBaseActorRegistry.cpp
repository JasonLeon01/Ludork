#include "GameMapBase.hpp"
#include "GameMapActorRegistry.hpp"

#include <Gameplay/Actor.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

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
