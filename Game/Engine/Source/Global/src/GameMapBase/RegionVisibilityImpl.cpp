#include "RegionVisibilityImpl.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>

namespace ludork::global::game_map_base_impl {

namespace {

bool intersectsCell(const std::array<sf::Vector2f, 4>& corners,
                    const sf::Vector2<double>& horizontal,
                    const sf::Vector2<double>& vertical, int x, int y,
                    int cellSize) {
    const std::array<sf::Vector2<double>, 4> axes = {
        sf::Vector2<double>{1.0, 0.0}, sf::Vector2<double>{0.0, 1.0},
        sf::Vector2<double>{-horizontal.y, horizontal.x},
        sf::Vector2<double>{-vertical.y, vertical.x}};
    const double halfSize = cellSize * 0.5;
    const sf::Vector2<double> centre{(x + 0.5) * cellSize,
                                     (y + 0.5) * cellSize};
    for (const sf::Vector2<double>& axis : axes) {
        double minimum = axis.x * corners[0].x + axis.y * corners[0].y;
        double maximum = minimum;
        for (std::size_t index = 1; index < corners.size(); ++index) {
            const double projection =
                axis.x * corners[index].x + axis.y * corners[index].y;
            minimum = std::min(minimum, projection);
            maximum = std::max(maximum, projection);
        }
        const double projectedCentre = axis.x * centre.x + axis.y * centre.y;
        const double radius = (std::abs(axis.x) + std::abs(axis.y)) * halfSize;
        if (maximum <= projectedCentre - radius ||
            minimum >= projectedCentre + radius) {
            return false;
        }
    }
    return true;
}

}  // namespace

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

bool RegionVisibilityImpl::isActorVisible(const sf::Vector2i& position,
                                          const sf::FloatRect& bounds,
                                          const sf::Transform& transform,
                                          int cellSize) const {
    if (!contains(position) || observerRegion_ < 0) {
        return false;
    }
    const int region = regions_[position.y][position.x];
    if (region >= 0) {
        return region == observerRegion_;
    }
    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f || cellSize <= 0) {
        return false;
    }
    const std::array<sf::Vector2f, 4> corners = {
        transform.transformPoint(bounds.position),
        transform.transformPoint(bounds.position +
                                 sf::Vector2f{bounds.size.x, 0}),
        transform.transformPoint(bounds.position + bounds.size),
        transform.transformPoint(bounds.position +
                                 sf::Vector2f{0, bounds.size.y})};
    sf::Vector2f minimum = corners[0];
    sf::Vector2f maximum = minimum;
    for (const sf::Vector2f& corner : corners) {
        if (!std::isfinite(corner.x) || !std::isfinite(corner.y)) {
            return false;
        }
        minimum.x = std::min(minimum.x, corner.x);
        minimum.y = std::min(minimum.y, corner.y);
        maximum.x = std::max(maximum.x, corner.x);
        maximum.y = std::max(maximum.y, corner.y);
    }
    const sf::Vector2<double> horizontal{
        static_cast<double>(corners[1].x) - corners[0].x,
        static_cast<double>(corners[1].y) - corners[0].y};
    const sf::Vector2<double> vertical{
        static_cast<double>(corners[3].x) - corners[0].x,
        static_cast<double>(corners[3].y) - corners[0].y};
    if (horizontal.x * vertical.y == horizontal.y * vertical.x) {
        return false;
    }
    const double size = cellSize;
    const int firstY =
        static_cast<int>(std::clamp(std::floor(minimum.y / size), 0.0,
                                    static_cast<double>(regions_.size())));
    const int endY =
        static_cast<int>(std::clamp(std::ceil(maximum.y / size), 0.0,
                                    static_cast<double>(regions_.size())));
    for (int y = firstY; y < endY; ++y) {
        const int firstX = static_cast<int>(
            std::clamp(std::floor(minimum.x / size), 0.0,
                       static_cast<double>(regions_[y].size())));
        const int endX = static_cast<int>(
            std::clamp(std::ceil(maximum.x / size), 0.0,
                       static_cast<double>(regions_[y].size())));
        for (int x = firstX; x < endX; ++x) {
            if (regions_[y][x] == observerRegion_ &&
                intersectsCell(corners, horizontal, vertical, x, y, cellSize)) {
                return true;
            }
        }
    }
    return false;
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
