#pragma once

#include <Gameplay/Actor.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct GameMapActorRegistryEntry {
    std::shared_ptr<Actor> owner;
    std::vector<std::string> layerOrder;
    std::unordered_set<std::string> liveLayers;
    bool createInitialised = false;
    bool componentInitialised = false;
};

class GameMapActorRegistry {
public:
    std::unordered_map<std::string, std::unordered_set<Actor*>> layerMembership;
    std::unordered_map<Actor*, GameMapActorRegistryEntry> entries;
    std::vector<Actor*> pendingCreateActors;
    std::vector<Actor*> pendingComponentActors;
    ActorUpdateBatch updateBatch;
    std::size_t batchDepth = 0;
    std::size_t viewSyncDeferDepth = 0;
    bool viewsDirty = false;
    bool initialising = false;
    bool flushing = false;
};
