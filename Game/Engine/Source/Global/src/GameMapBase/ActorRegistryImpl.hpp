#pragma once

#include <Gameplay/Actor.hpp>
#include <Gameplay/ActorMapService.hpp>
#include <Gameplay/ActorUpdateBatch.hpp>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ludork::global::game_map_base_impl {

class OccupancyIndexImpl;
class ActorRegistryImpl {
public:
    using ActorPtr = std::shared_ptr<Actor>;
    using ActorDict = std::unordered_map<std::string, std::vector<ActorPtr>>;
    const ActorDict& actors() const;
    const ActorDict& materialActors() const;
    const ActorPtr& playerActor() const;
    const std::unordered_map<Actor*, std::string>& actorLayers() const;
    void markViewsDirty();
    void destroyActor(Actor& actor);
    void releaseEmitters() noexcept;
    void syncActorsRef(const ActorDict& actors, OccupancyIndexImpl& occupancy);
    void syncMaterialActorsRef(const ActorDict& actors);
    bool registerLayerActor(ActorPtr actor, const std::string& layer);
    std::optional<std::string> getRegisteredActorLayer(Actor& actor) const;
    void beginActorBatch();
    void endActorBatch();
    void flushActorChanges();
    void withDeferredActorViewSync(std::function<void()> handler);
    void drainActorLifecycle(std::function<void(Actor&)> createHandler,
                             std::function<void(Actor&)> componentHandler);
    void syncActorViews(const ActorDict& actors,
                        const std::shared_ptr<ActorMapService>& mapOwner,
                        OccupancyIndexImpl& occupancy);
    bool forgetActors(const std::vector<ActorPtr>& actors,
                      OccupancyIndexImpl& occupancy,
                      const std::optional<sf::Vector2u>& worldSize);
    void updateActors(float deltaTime);
    void lateUpdateActors(float deltaTime);
    void fixedUpdateActors(float fixedDelta);
    void setActorListUpdater(std::function<void()> updater);
    void setActorDestroyer(std::function<void(Actor&)> destroyer);
    void setPlayerActor(ActorPtr actor);

private:
    struct Entry {
        std::shared_ptr<Actor> owner;
        std::vector<std::string> layerOrder;
        std::unordered_set<std::string> liveLayers;
        bool createInitialised = false;
        bool componentInitialised = false;
    };
    static void rememberLayer(Entry& entry, const std::string& layer);
    ActorDict actorsRef_;
    ActorDict materialActorsRef_;
    std::unordered_map<Actor*, std::string> actorLayerRef_;
    ActorPtr playerActor_;
    std::function<void()> actorListUpdater_;
    std::function<void(Actor&)> actorDestroyer_;
    std::unordered_map<std::string, std::unordered_set<Actor*>> layerMembership;
    std::unordered_map<Actor*, Entry> entries;
    std::vector<Actor*> pendingCreateActors;
    std::vector<Actor*> pendingComponentActors;
    ActorUpdateBatch updateBatch;
    std::size_t batchDepth = 0;
    std::size_t viewSyncDeferDepth = 0;
    bool viewsDirty = false;
    bool initialising = false;
    bool flushing = false;
};
}  // namespace ludork::global::game_map_base_impl
