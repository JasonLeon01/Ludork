#include "ActorRegistryImpl.hpp"
#include "ActorRegistryGuards.hpp"
#include "OccupancyIndexImpl.hpp"
#include <algorithm>
#include <stdexcept>
namespace ludork::global::game_map_base_impl {

void ActorRegistryImpl::syncActorsRef(const ActorDict& actors,
                                      OccupancyIndexImpl& occupancy) {
    occupancy.clearActorOccupancy();
    occupancy.clearRegisteredCells();
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
}

void ActorRegistryImpl::syncMaterialActorsRef(const ActorDict& actors) {
    std::unordered_set<Actor*> nextActors;
    for (const auto& [_, actorList] : actors) {
        for (const ActorPtr& actor : actorList) {
            nextActors.insert(actor.get());
        }
    }
    for (const auto& [_, actorList] : materialActorsRef_) {
        for (const ActorPtr& actor : actorList) {
            if (actor && !nextActors.contains(actor.get())) {
                actor->releaseBillboard();
            }
        }
    }
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

bool ActorRegistryImpl::registerLayerActor(ActorPtr actor,
                                           const std::string& layer) {
    if (!actor) {
        return false;
    }
    Actor* actorPointer = actor.get();
    auto& membership = layerMembership[layer];
    if (!membership.insert(actorPointer).second) {
        return false;
    }

    Entry& entry = entries[actorPointer];
    entry.owner = std::move(actor);
    rememberLayer(entry, layer);
    entry.liveLayers.insert(layer);
    for (const std::string& registeredLayer : entry.layerOrder) {
        if (entry.liveLayers.contains(registeredLayer)) {
            actorLayerRef_[actorPointer] = registeredLayer;
            break;
        }
    }
    pendingCreateActors.push_back(actorPointer);
    pendingComponentActors.push_back(actorPointer);
    viewsDirty = true;
    return true;
}

std::optional<std::string> ActorRegistryImpl::getRegisteredActorLayer(
    Actor& actor) const {
    const auto layerIt = actorLayerRef_.find(&actor);
    if (layerIt == actorLayerRef_.end()) {
        return std::nullopt;
    }
    return layerIt->second;
}

void ActorRegistryImpl::beginActorBatch() {
    ++batchDepth;
}

void ActorRegistryImpl::endActorBatch() {
    if (batchDepth == 0) {
        throw std::logic_error("Actor batch is not active");
    }
    --batchDepth;
    if (batchDepth == 0) {
        flushActorChanges();
    }
}

void ActorRegistryImpl::flushActorChanges() {
    if (!viewsDirty || batchDepth != 0 || viewSyncDeferDepth != 0 ||
        initialising || flushing || !actorListUpdater_) {
        return;
    }
    ludork::global::game_map_base_impl::BoolResetGuard flushingGuard(flushing);
    actorListUpdater_();
}

void ActorRegistryImpl::withDeferredActorViewSync(
    std::function<void()> handler) {
    viewsDirty = true;

    {
        ludork::global::game_map_base_impl::DepthGuard deferGuard(
            viewSyncDeferDepth);
        handler();
    }
    flushActorChanges();
}

void ActorRegistryImpl::drainActorLifecycle(
    std::function<void(Actor&)> createHandler,
    std::function<void(Actor&)> componentHandler) {
    if (initialising || batchDepth != 0) {
        return;
    }
    {
        ludork::global::game_map_base_impl::BoolResetGuard initialisingGuard(
            initialising);
        while (true) {
            bool createdAny = false;
            while (!pendingCreateActors.empty()) {
                std::vector<Actor*> pendingActors =
                    std::move(pendingCreateActors);
                pendingCreateActors.clear();
                std::vector<Actor*> liveActors;
                liveActors.reserve(pendingActors.size());
                for (Actor* actor : pendingActors) {
                    const auto entryIt = entries.find(actor);
                    if (entryIt == entries.end() ||
                        entryIt->second.createInitialised ||
                        actor->isDestroyed() ||
                        !actorLayerRef_.contains(actor)) {
                        continue;
                    }
                    liveActors.push_back(actor);
                }
                for (Actor* actor : liveActors) {
                    Entry& entry = entries.at(actor);
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
                std::move(pendingComponentActors);
            pendingComponentActors.clear();
            std::vector<Actor*> liveActors;
            liveActors.reserve(pendingActors.size());
            for (Actor* actor : pendingActors) {
                const auto entryIt = entries.find(actor);
                if (entryIt == entries.end() ||
                    entryIt->second.componentInitialised ||
                    actor->isDestroyed() || !actorLayerRef_.contains(actor)) {
                    continue;
                }
                liveActors.push_back(actor);
            }
            for (Actor* actor : liveActors) {
                Entry& entry = entries.at(actor);
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

void ActorRegistryImpl::syncActorViews(
    const ActorDict& actors, const std::shared_ptr<ActorMapService>& mapOwner,
    OccupancyIndexImpl& occupancy) {
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
            Entry& entry = entries[actorPointer];
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
            Entry& entry = entries[actorPointer];
            entry.owner = actor;
            for (const ActorPtr& child : actor->getChildren()) {
                if (child) {
                    queue.push_back(child);
                }
            }
        }
    }

    updateBatch.syncActors(updateActors);

    for (auto& [_, entry] : entries) {
        entry.liveLayers.clear();
    }
    for (const auto& [layerName, membership] : nextLayerMembership) {
        for (Actor* actor : membership) {
            Entry& entry = entries.at(actor);
            rememberLayer(entry, layerName);
            entry.liveLayers.insert(layerName);
        }
    }

    std::unordered_map<Actor*, std::string> nextActorLayerRef;
    nextActorLayerRef.reserve(firstExpandedLayer.size());
    for (const auto& [actor, entry] : entries) {
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
    for (auto iterator = entries.begin(); iterator != entries.end();) {
        Actor* actor = iterator->first;
        if (!nextActorLayerRef.contains(actor) && iterator->second.owner) {
            iterator->second.owner->releaseBillboard();
        }
        if (actor != playerActor_.get() && !nextActorLayerRef.contains(actor) &&
            iterator->second.owner && iterator->second.owner->isDestroyed()) {
            destroyedEntries.insert(actor);
            iterator = entries.erase(iterator);
        } else {
            ++iterator;
        }
    }
    if (!destroyedEntries.empty()) {
        std::erase_if(pendingCreateActors, [&](Actor* actor) {
            return destroyedEntries.contains(actor);
        });
        std::erase_if(pendingComponentActors, [&](Actor* actor) {
            return destroyedEntries.contains(actor);
        });
    }

    occupancy.clearActorOccupancy();
    occupancy.clearRegisteredCells();
    actorsRef_ = std::move(nextActorsRef);
    materialActorsRef_ = std::move(nextMaterialActorsRef);
    actorLayerRef_ = std::move(nextActorLayerRef);
    layerMembership = std::move(nextLayerMembership);

    viewsDirty = false;
}

bool ActorRegistryImpl::forgetActors(
    const std::vector<ActorPtr>& actors, OccupancyIndexImpl& occupancy,
    const std::optional<sf::Vector2u>& worldSize) {
    std::unordered_set<Actor*> targets;
    targets.reserve(actors.size());
    for (const ActorPtr& actor : actors) {
        if (actor) {
            targets.insert(actor.get());
        }
    }
    if (targets.empty()) {
        return false;
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
        if (occupancy.registeredCells(actor) != nullptr) {
            occupancy.unregisterActorOccupancy(*actor, worldSize);
        }
        actor->releaseEmitter();
        actor->releaseBillboard();
        actorLayerRef_.erase(actor);
        entries.erase(actor);
    }
    for (auto iterator = layerMembership.begin();
         iterator != layerMembership.end();) {
        for (Actor* actor : targets) {
            iterator->second.erase(actor);
        }
        if (iterator->second.empty()) {
            iterator = layerMembership.erase(iterator);
        } else {
            ++iterator;
        }
    }
    std::erase_if(pendingCreateActors, [&](Actor* actor) {
        return targets.contains(actor);
    });
    std::erase_if(pendingComponentActors, [&](Actor* actor) {
        return targets.contains(actor);
    });

    std::vector<ActorPtr> updateActors;
    for (const auto& [_, actorList] : materialActorsRef_) {
        updateActors.insert(updateActors.end(), actorList.begin(),
                            actorList.end());
    }
    updateBatch.syncActors(updateActors);

    return true;
}

void ActorRegistryImpl::updateActors(float deltaTime) {
    updateBatch.update(deltaTime);
}

void ActorRegistryImpl::lateUpdateActors(float deltaTime) {
    updateBatch.lateUpdate(deltaTime);
}

void ActorRegistryImpl::fixedUpdateActors(float fixedDelta) {
    updateBatch.fixedUpdate(fixedDelta);
}

void ActorRegistryImpl::setActorListUpdater(std::function<void()> updater) {
    actorListUpdater_ = std::move(updater);
}

void ActorRegistryImpl::setActorDestroyer(
    std::function<void(Actor&)> destroyer) {
    actorDestroyer_ = std::move(destroyer);
}

void ActorRegistryImpl::setPlayerActor(ActorPtr actor) {
    playerActor_ = std::move(actor);
}

void ActorRegistryImpl::rememberLayer(Entry& entry, const std::string& layer) {
    if (std::find(entry.layerOrder.begin(), entry.layerOrder.end(), layer) ==
        entry.layerOrder.end()) {
        entry.layerOrder.push_back(layer);
    }
}
const ActorRegistryImpl::ActorDict& ActorRegistryImpl::actors() const {
    return actorsRef_;
}
const ActorRegistryImpl::ActorDict& ActorRegistryImpl::materialActors() const {
    return materialActorsRef_;
}
const ActorRegistryImpl::ActorPtr& ActorRegistryImpl::playerActor() const {
    return playerActor_;
}
const std::unordered_map<Actor*, std::string>& ActorRegistryImpl::actorLayers()
    const {
    return actorLayerRef_;
}
void ActorRegistryImpl::markViewsDirty() {
    viewsDirty = true;
}
void ActorRegistryImpl::destroyActor(Actor& actor) {
    if (actorDestroyer_) {
        actorDestroyer_(actor);
    }
}

void ActorRegistryImpl::releaseEmitters() noexcept {
    for (const auto& [_, entry] : entries) {
        if (entry.owner != nullptr) {
            entry.owner->releaseEmitter();
        }
    }
}

void ActorRegistryImpl::releaseBillboards() noexcept {
    for (const auto& [_, entry] : entries) {
        if (entry.owner != nullptr) {
            entry.owner->releaseBillboard();
        }
    }
}

}  // namespace ludork::global::game_map_base_impl
