#include "RegionVisibilityImpl.hpp"

#include <array>
#include <queue>

namespace ludork::global::game_map_base_impl {

void RegionVisibilityImpl::rebuild(
    const std::vector<std::vector<bool>>& passable) {
    regions_.clear();
    replacementSources_.clear();
    regions_.reserve(passable.size());
    replacementSources_.reserve(passable.size());
    observerRegion_ = -1;
    for (const auto& row : passable) {
        regions_.emplace_back(row.size(), -1);
        replacementSources_.emplace_back(row.size(), std::nullopt);
    }

    const std::array<sf::Vector2i, 4> directions = {
        sf::Vector2i{0, -1}, sf::Vector2i{-1, 0}, sf::Vector2i{1, 0},
        sf::Vector2i{0, 1}};
    std::queue<sf::Vector2i> regionQueue;
    std::queue<sf::Vector2i> sourceQueue;
    int nextRegion = 0;
    for (std::size_t y = 0; y < passable.size(); ++y) {
        for (std::size_t x = 0; x < passable[y].size(); ++x) {
            const sf::Vector2i position{static_cast<int>(x),
                                        static_cast<int>(y)};
            if (!passable[y][x]) {
                replacementSources_[y][x] = position;
                sourceQueue.push(position);
                continue;
            }
            if (regions_[y][x] >= 0) {
                continue;
            }
            const int region = nextRegion++;
            regions_[y][x] = region;
            regionQueue.push(position);
            while (!regionQueue.empty()) {
                const sf::Vector2i current = regionQueue.front();
                regionQueue.pop();
                for (const sf::Vector2i& direction : directions) {
                    const sf::Vector2i neighbour = current + direction;
                    if (!contains(neighbour) ||
                        !passable[neighbour.y][neighbour.x] ||
                        regions_[neighbour.y][neighbour.x] >= 0) {
                        continue;
                    }
                    regions_[neighbour.y][neighbour.x] = region;
                    regionQueue.push(neighbour);
                }
            }
        }
    }

    // Row-major seeds and FIFO expansion preserve source order at every
    // distance. Cross both walls and floors: distance is geometric, independent
    // of passability.
    while (!sourceQueue.empty()) {
        const sf::Vector2i current = sourceQueue.front();
        sourceQueue.pop();
        for (const sf::Vector2i& direction : directions) {
            const sf::Vector2i neighbour = current + direction;
            if (!contains(neighbour) ||
                replacementSources_[neighbour.y][neighbour.x]) {
                continue;
            }
            replacementSources_[neighbour.y][neighbour.x] =
                replacementSources_[current.y][current.x];
            sourceQueue.push(neighbour);
        }
    }
}

bool RegionVisibilityImpl::setObserver(std::optional<sf::Vector2i> position) {
    const int region = position && contains(*position)
                           ? regions_[position->y][position->x]
                           : -1;
    if (observerRegion_ == region) {
        return false;
    }
    observerRegion_ = region;
    return true;
}

bool RegionVisibilityImpl::isCellVisible(const sf::Vector2i& position) const {
    if (!contains(position)) {
        return false;
    }
    const int region = regions_[position.y][position.x];
    return region < 0 || region == observerRegion_;
}

std::optional<sf::Vector2i> RegionVisibilityImpl::replacementSource(
    const sf::Vector2i& position) const {
    if (isCellVisible(position) || !contains(position)) {
        return std::nullopt;
    }
    return replacementSources_[position.y][position.x];
}

bool RegionVisibilityImpl::contains(const sf::Vector2i& position) const {
    return position.y >= 0 &&
           static_cast<std::size_t>(position.y) < regions_.size() &&
           position.x >= 0 &&
           static_cast<std::size_t>(position.x) < regions_[position.y].size();
}

}  // namespace ludork::global::game_map_base_impl
