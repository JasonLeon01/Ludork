#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

BIND_ENUM()
enum class WorldRegionState {
    Unloaded,
    Reading,
    Prepared,
    Active,
    Dormant,
};

BIND_ENUM()
enum class WorldRegionDemand {
    None,
    Prepared,
    Active,
};

BIND_CLASS(copyable = true)
struct LUDORK_GLOBAL_API WorldStreamingStats {
    BIND_PROPERTY()
    int Unloaded = 0;

    BIND_PROPERTY()
    int Reading = 0;

    BIND_PROPERTY()
    int Prepared = 0;

    BIND_PROPERTY()
    int Active = 0;

    BIND_PROPERTY()
    int Dormant = 0;

    BIND_PROPERTY()
    int queued = 0;

    BIND_PROPERTY()
    std::int64_t cacheBytes = 0;
};

BIND_CLASS()
class LUDORK_GLOBAL_API WorldStreamingState {
public:
    BIND_INIT()
    WorldStreamingState(std::vector<sf::IntRect> regionRects,
                        int nonActiveRegionLimit,
                        std::int64_t nonActiveByteLimit);
    ~WorldStreamingState();

    WorldStreamingState(const WorldStreamingState&) = delete;
    WorldStreamingState& operator=(const WorldStreamingState&) = delete;

    BIND_METHOD(metadata = false)
    sf::Vector2f updateCameraCenter(const sf::Vector2f& cameraCenter);

    BIND_METHOD(metadata = false)
    void updateDemand(const sf::IntRect& activeRect,
                      const sf::IntRect& preparedRect,
                      const sf::Vector2f& cameraCenter,
                      const std::vector<int>& actorDemandRegions);

    BIND_METHOD(metadata = false)
    WorldRegionState getRegionState(int regionIndex) const;

    BIND_METHOD(metadata = false)
    WorldRegionDemand getRegionDemand(int regionIndex) const;

    BIND_METHOD(metadata = false)
    bool isRegionDemanded(int regionIndex) const;

    BIND_METHOD(metadata = false)
    bool isRegionLoaded(int regionIndex) const;

    BIND_METHOD(metadata = false)
    void requestRegion(int regionIndex);

    BIND_METHOD(metadata = false)
    std::vector<int> takeReadBatch(int maximumCount);

    BIND_METHOD(metadata = false)
    void cancelRead(int regionIndex, bool requeue);

    BIND_METHOD(metadata = false)
    void beginPublish(int regionIndex, bool forceActivate);

    BIND_METHOD(metadata = false)
    std::optional<int> takePublishItem();

    BIND_METHOD(metadata = false)
    void deferPublish(int regionIndex);

    BIND_METHOD(metadata = false)
    void cancelPublish(int regionIndex, bool requeueRead);

    BIND_METHOD(metadata = false)
    void completePublish(int regionIndex, std::int64_t payloadBytes,
                         bool activate);

    BIND_METHOD(metadata = false)
    void markActive(int regionIndex);

    BIND_METHOD(metadata = false)
    void markInactive(int regionIndex, WorldRegionState state);

    BIND_METHOD(metadata = false)
    void markEvicted(int regionIndex);

    BIND_METHOD(metadata = false)
    std::int64_t getRegionPayloadBytes(int regionIndex) const;

    BIND_METHOD(metadata = false)
    std::vector<int> getEvictionList() const;

    BIND_METHOD(metadata = false)
    WorldStreamingStats getStats(int backgroundQueueDepth) const;

    BIND_METHOD(metadata = false)
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
