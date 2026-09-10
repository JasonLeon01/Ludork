#pragma once

#include <World/WorldStreamingState.hpp>

#include <limits>

struct WorldStreamingState::Impl {
    struct Region {
        sf::IntRect rect;
        WorldRegionState state = WorldRegionState::Unloaded;
        WorldRegionDemand demand = WorldRegionDemand::None;
        std::uint64_t demandGeneration = 0;
        double lastUsed = -std::numeric_limits<double>::infinity();
        std::int64_t payloadBytes = 0;
        bool actorDemand = false;
        bool preparedEvicted = false;
        bool readQueued = false;
        bool publishing = false;
        bool publishQueued = false;
        bool forceActivate = false;
    };

    explicit Impl(std::vector<sf::IntRect> regionRects, int regionLimit,
                  std::int64_t byteLimit);

    Region& require(int regionIndex);

    const Region& require(int regionIndex) const;

    bool demanded(const Region& region) const;

    bool mayRead(const Region& region) const;

    void queueRead(std::size_t index);

    double distanceSquared(const Region& region) const;

    bool demandedBefore(int leftIndex, int rightIndex) const;

    void sortQueues();

    void rebuildQueues();

    std::vector<Region> regions;
    std::vector<int> readQueue;
    std::vector<int> publishQueue;
    std::uint64_t demandGeneration = 0;
    sf::Vector2f cameraCenter;
    std::optional<sf::Vector2f> previousCameraCenter;
    int nonActiveRegionLimit = 0;
    std::int64_t nonActiveByteLimit = 0;
};
