#include <World/WorldStreamingState.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace {

bool intersects(const sf::IntRect& left, const sf::IntRect& right) {
    const std::int64_t leftRight =
        static_cast<std::int64_t>(left.position.x) + left.size.x;
    const std::int64_t leftBottom =
        static_cast<std::int64_t>(left.position.y) + left.size.y;
    const std::int64_t rightRight =
        static_cast<std::int64_t>(right.position.x) + right.size.x;
    const std::int64_t rightBottom =
        static_cast<std::int64_t>(right.position.y) + right.size.y;
    return left.position.x < rightRight && right.position.x < leftRight &&
           left.position.y < rightBottom && right.position.y < leftBottom;
}

double timestamp() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

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
                  std::int64_t byteLimit)
        : nonActiveRegionLimit(regionLimit), nonActiveByteLimit(byteLimit) {
        if (nonActiveRegionLimit < 0 || nonActiveByteLimit < 0) {
            throw std::invalid_argument(
                "World streaming cache limits must not be negative");
        }
        regions.reserve(regionRects.size());
        for (const sf::IntRect& rect : regionRects) {
            if (rect.size.x <= 0 || rect.size.y <= 0) {
                throw std::invalid_argument(
                    "World streaming region rectangles must be positive");
            }
            regions.push_back({.rect = rect});
        }
    }

    Region& require(int regionIndex) {
        if (regionIndex <= 0 ||
            regionIndex > static_cast<int>(regions.size())) {
            throw std::out_of_range(
                "World streaming region index is out of range");
        }
        return regions[static_cast<std::size_t>(regionIndex - 1)];
    }

    const Region& require(int regionIndex) const {
        if (regionIndex <= 0 ||
            regionIndex > static_cast<int>(regions.size())) {
            throw std::out_of_range(
                "World streaming region index is out of range");
        }
        return regions[static_cast<std::size_t>(regionIndex - 1)];
    }

    bool demanded(const Region& region) const {
        return (region.demandGeneration == demandGeneration &&
                region.demand != WorldRegionDemand::None) ||
               region.actorDemand;
    }

    bool mayRead(const Region& region) const {
        return demanded(region) && region.state == WorldRegionState::Unloaded &&
               !region.publishing &&
               !(region.demand == WorldRegionDemand::Prepared &&
                 region.preparedEvicted);
    }

    void queueRead(std::size_t index) {
        Region& region = regions[index];
        if (!region.readQueued && mayRead(region)) {
            region.readQueued = true;
            readQueue.push_back(static_cast<int>(index + 1));
        }
    }

    double distanceSquared(const Region& region) const {
        const double x = static_cast<double>(region.rect.position.x) +
                         static_cast<double>(region.rect.size.x) * 0.5 -
                         cameraCenter.x;
        const double y = static_cast<double>(region.rect.position.y) +
                         static_cast<double>(region.rect.size.y) * 0.5 -
                         cameraCenter.y;
        return x * x + y * y;
    }

    bool demandedBefore(int leftIndex, int rightIndex) const {
        const Region& left = require(leftIndex);
        const Region& right = require(rightIndex);
        const bool leftActive = left.demand == WorldRegionDemand::Active;
        const bool rightActive = right.demand == WorldRegionDemand::Active;
        if (leftActive != rightActive) {
            return leftActive;
        }
        const double leftDistance = distanceSquared(left);
        const double rightDistance = distanceSquared(right);
        if (leftDistance != rightDistance) {
            return leftDistance < rightDistance;
        }
        return leftIndex < rightIndex;
    }

    void sortQueues() {
        const auto before = [&](int left, int right) {
            return demandedBefore(left, right);
        };
        std::stable_sort(readQueue.begin(), readQueue.end(), before);
        std::stable_sort(publishQueue.begin(), publishQueue.end(), before);
    }

    void rebuildQueues() {
        std::vector<int> reads;
        reads.reserve(readQueue.size() + regions.size());
        for (std::size_t index = 0; index < regions.size(); ++index) {
            Region& region = regions[index];
            region.readQueued = false;
            if (mayRead(region)) {
                region.readQueued = true;
                reads.push_back(static_cast<int>(index + 1));
            }
        }
        readQueue = std::move(reads);

        std::vector<int> publishes;
        publishes.reserve(publishQueue.size());
        for (const int index : publishQueue) {
            Region& region = require(index);
            if (region.publishing &&
                (demanded(region) || region.forceActivate)) {
                publishes.push_back(index);
            } else {
                region.publishQueued = false;
            }
        }
        publishQueue = std::move(publishes);
        sortQueues();
    }

    std::vector<Region> regions;
    std::vector<int> readQueue;
    std::vector<int> publishQueue;
    std::uint64_t demandGeneration = 0;
    sf::Vector2f cameraCenter;
    std::optional<sf::Vector2f> previousCameraCenter;
    int nonActiveRegionLimit = 0;
    std::int64_t nonActiveByteLimit = 0;
};

WorldStreamingState::WorldStreamingState(std::vector<sf::IntRect> regionRects,
                                         int nonActiveRegionLimit,
                                         std::int64_t nonActiveByteLimit)
    : impl_(std::make_unique<Impl>(std::move(regionRects), nonActiveRegionLimit,
                                   nonActiveByteLimit)) {}

WorldStreamingState::~WorldStreamingState() = default;

sf::Vector2f WorldStreamingState::updateCameraCenter(
    const sf::Vector2f& cameraCenter) {
    const sf::Vector2f movement =
        impl_->previousCameraCenter.has_value()
            ? cameraCenter - *impl_->previousCameraCenter
            : sf::Vector2f{};
    impl_->previousCameraCenter = cameraCenter;
    impl_->cameraCenter = cameraCenter;
    return movement;
}

void WorldStreamingState::updateDemand(
    const sf::IntRect& activeRect, const sf::IntRect& preparedRect,
    const sf::Vector2f& cameraCenter,
    const std::vector<int>& actorDemandRegions) {
    ++impl_->demandGeneration;
    impl_->cameraCenter = cameraCenter;
    for (Impl::Region& region : impl_->regions) {
        region.actorDemand = false;
        if (intersects(region.rect, activeRect)) {
            region.demand = WorldRegionDemand::Active;
        } else if (intersects(region.rect, preparedRect)) {
            region.demand = WorldRegionDemand::Prepared;
        } else {
            region.demand = WorldRegionDemand::None;
        }
        if (region.demand != WorldRegionDemand::Prepared) {
            region.preparedEvicted = false;
        }
        region.demandGeneration = impl_->demandGeneration;
    }
    for (const int regionIndex : actorDemandRegions) {
        impl_->require(regionIndex).actorDemand = true;
    }
    impl_->rebuildQueues();
}

WorldRegionState WorldStreamingState::getRegionState(int regionIndex) const {
    return impl_->require(regionIndex).state;
}

WorldRegionDemand WorldStreamingState::getRegionDemand(int regionIndex) const {
    return impl_->require(regionIndex).demand;
}

bool WorldStreamingState::isRegionDemanded(int regionIndex) const {
    return impl_->demanded(impl_->require(regionIndex));
}

bool WorldStreamingState::isRegionLoaded(int regionIndex) const {
    const WorldRegionState state = impl_->require(regionIndex).state;
    return state == WorldRegionState::Prepared ||
           state == WorldRegionState::Active ||
           state == WorldRegionState::Dormant;
}

void WorldStreamingState::requestRegion(int regionIndex) {
    Impl::Region& region = impl_->require(regionIndex);
    region.actorDemand = true;
    impl_->queueRead(static_cast<std::size_t>(regionIndex - 1));
    impl_->sortQueues();
}

std::vector<int> WorldStreamingState::takeReadBatch(int maximumCount) {
    if (maximumCount < 0) {
        throw std::invalid_argument(
            "World streaming read batch size must not be negative");
    }
    std::vector<int> result;
    result.reserve(static_cast<std::size_t>(maximumCount));
    while (static_cast<int>(result.size()) < maximumCount &&
           !impl_->readQueue.empty()) {
        const int index = impl_->readQueue.front();
        impl_->readQueue.erase(impl_->readQueue.begin());
        Impl::Region& region = impl_->require(index);
        region.readQueued = false;
        if (!impl_->mayRead(region)) {
            continue;
        }
        region.state = WorldRegionState::Reading;
        result.push_back(index);
    }
    return result;
}

void WorldStreamingState::cancelRead(int regionIndex, bool requeue) {
    Impl::Region& region = impl_->require(regionIndex);
    if (region.state == WorldRegionState::Reading && !region.publishing) {
        region.state = WorldRegionState::Unloaded;
    }
    if (requeue) {
        impl_->queueRead(static_cast<std::size_t>(regionIndex - 1));
        impl_->sortQueues();
    }
}

void WorldStreamingState::beginPublish(int regionIndex, bool forceActivate) {
    Impl::Region& region = impl_->require(regionIndex);
    if (region.publishing) {
        region.forceActivate = region.forceActivate || forceActivate;
        if (!region.publishQueued) {
            region.publishQueued = true;
            impl_->publishQueue.push_back(regionIndex);
            impl_->sortQueues();
        }
        return;
    }
    region.state = WorldRegionState::Reading;
    region.publishing = true;
    region.forceActivate = forceActivate;
    if (!region.publishQueued) {
        region.publishQueued = true;
        impl_->publishQueue.push_back(regionIndex);
        impl_->sortQueues();
    }
}

std::optional<int> WorldStreamingState::takePublishItem() {
    while (!impl_->publishQueue.empty()) {
        const int index = impl_->publishQueue.front();
        impl_->publishQueue.erase(impl_->publishQueue.begin());
        Impl::Region& region = impl_->require(index);
        region.publishQueued = false;
        if (region.publishing &&
            (impl_->demanded(region) || region.forceActivate)) {
            return index;
        }
    }
    return std::nullopt;
}

void WorldStreamingState::deferPublish(int regionIndex) {
    Impl::Region& region = impl_->require(regionIndex);
    if (region.publishing && !region.publishQueued &&
        (impl_->demanded(region) || region.forceActivate)) {
        region.publishQueued = true;
        impl_->publishQueue.push_back(regionIndex);
    }
}

void WorldStreamingState::cancelPublish(int regionIndex, bool requeueRead) {
    Impl::Region& region = impl_->require(regionIndex);
    region.publishing = false;
    region.publishQueued = false;
    region.forceActivate = false;
    if (region.state == WorldRegionState::Reading) {
        region.state = WorldRegionState::Unloaded;
    }
    std::erase(impl_->publishQueue, regionIndex);
    if (requeueRead) {
        impl_->queueRead(static_cast<std::size_t>(regionIndex - 1));
        impl_->sortQueues();
    }
}

void WorldStreamingState::completePublish(int regionIndex,
                                          std::int64_t payloadBytes,
                                          bool activate) {
    if (payloadBytes < 0) {
        throw std::invalid_argument(
            "World region payload size must not be negative");
    }
    Impl::Region& region = impl_->require(regionIndex);
    region.publishing = false;
    region.publishQueued = false;
    region.forceActivate = false;
    region.payloadBytes = payloadBytes;
    region.preparedEvicted = false;
    region.lastUsed = timestamp();
    region.state =
        activate ? WorldRegionState::Active : WorldRegionState::Prepared;
    std::erase(impl_->publishQueue, regionIndex);
}

void WorldStreamingState::markActive(int regionIndex) {
    Impl::Region& region = impl_->require(regionIndex);
    if (!isRegionLoaded(regionIndex)) {
        throw std::logic_error("Unloaded world region cannot become Active");
    }
    region.state = WorldRegionState::Active;
    region.lastUsed = timestamp();
}

void WorldStreamingState::markInactive(int regionIndex,
                                       WorldRegionState state) {
    if (state != WorldRegionState::Prepared &&
        state != WorldRegionState::Dormant) {
        throw std::invalid_argument(
            "Inactive world region state must be Prepared or Dormant");
    }
    Impl::Region& region = impl_->require(regionIndex);
    if (!isRegionLoaded(regionIndex)) {
        throw std::logic_error("Unloaded world region cannot become inactive");
    }
    region.state = state;
    region.lastUsed = timestamp();
}

void WorldStreamingState::markEvicted(int regionIndex) {
    Impl::Region& region = impl_->require(regionIndex);
    region.preparedEvicted = region.demand == WorldRegionDemand::Prepared;
    region.state = WorldRegionState::Unloaded;
    region.payloadBytes = 0;
    region.lastUsed = -std::numeric_limits<double>::infinity();
    impl_->queueRead(static_cast<std::size_t>(regionIndex - 1));
    impl_->sortQueues();
}

std::int64_t WorldStreamingState::getRegionPayloadBytes(int regionIndex) const {
    return impl_->require(regionIndex).payloadBytes;
}

std::vector<int> WorldStreamingState::getEvictionList() const {
    int cacheCount = 0;
    std::int64_t cacheBytes = 0;
    std::vector<int> dormant;
    std::vector<int> prepared;
    for (std::size_t index = 0; index < impl_->regions.size(); ++index) {
        const Impl::Region& region = impl_->regions[index];
        if (region.state == WorldRegionState::Prepared ||
            region.state == WorldRegionState::Dormant) {
            ++cacheCount;
            cacheBytes += region.payloadBytes;
            if (!region.actorDemand) {
                (region.state == WorldRegionState::Dormant ? dormant : prepared)
                    .push_back(static_cast<int>(index + 1));
            }
        }
    }
    if (cacheCount <= impl_->nonActiveRegionLimit &&
        cacheBytes <= impl_->nonActiveByteLimit) {
        return {};
    }
    const auto usedBefore = [&](int left, int right) {
        const double leftTime = impl_->require(left).lastUsed;
        const double rightTime = impl_->require(right).lastUsed;
        return leftTime == rightTime ? left < right : leftTime < rightTime;
    };
    std::stable_sort(dormant.begin(), dormant.end(), usedBefore);
    std::stable_sort(prepared.begin(), prepared.end(), usedBefore);
    dormant.insert(dormant.end(), prepared.begin(), prepared.end());

    std::vector<int> result;
    for (const int index : dormant) {
        if (cacheCount <= impl_->nonActiveRegionLimit &&
            cacheBytes <= impl_->nonActiveByteLimit) {
            break;
        }
        const Impl::Region& region = impl_->require(index);
        result.push_back(index);
        --cacheCount;
        cacheBytes =
            std::max<std::int64_t>(0, cacheBytes - region.payloadBytes);
    }
    return result;
}

WorldStreamingStats WorldStreamingState::getStats(
    int backgroundQueueDepth) const {
    WorldStreamingStats result;
    result.queued =
        static_cast<int>(impl_->readQueue.size() + impl_->publishQueue.size()) +
        std::max(0, backgroundQueueDepth);
    for (const Impl::Region& region : impl_->regions) {
        switch (region.state) {
            case WorldRegionState::Unloaded:
                ++result.Unloaded;
                break;
            case WorldRegionState::Reading:
                ++result.Reading;
                break;
            case WorldRegionState::Prepared:
                ++result.Prepared;
                result.cacheBytes += region.payloadBytes;
                break;
            case WorldRegionState::Active:
                ++result.Active;
                break;
            case WorldRegionState::Dormant:
                ++result.Dormant;
                result.cacheBytes += region.payloadBytes;
                break;
        }
    }
    return result;
}

void WorldStreamingState::reset() {
    impl_->readQueue.clear();
    impl_->publishQueue.clear();
    impl_->demandGeneration = 0;
    impl_->cameraCenter = {};
    impl_->previousCameraCenter.reset();
    for (Impl::Region& region : impl_->regions) {
        const sf::IntRect rect = region.rect;
        region = {};
        region.rect = rect;
    }
}
