#include "PathfindingRuntime.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <unordered_set>

namespace ludork::global::game_map_base_impl {
namespace {

struct GridPointHash {
    std::size_t operator()(const GridPoint& value) const {
        const std::size_t xHash = std::hash<int>{}(value.first);
        const std::size_t yHash = std::hash<int>{}(value.second);
        return xHash ^ (yHash + 0x9e3779b9 + (xHash << 6) + (xHash >> 2));
    }
};

bool inBounds(int x, int y, unsigned int width, unsigned int height) {
    return x >= 0 && y >= 0 && static_cast<unsigned int>(x) < width &&
           static_cast<unsigned int>(y) < height;
}

int nodeFScore(const std::map<GridPoint, int>& scores, const GridPoint& node) {
    const auto iterator = scores.find(node);
    return iterator == scores.end() ? 1 << 30 : iterator->second;
}

}  // namespace

std::vector<GridPoint> findPath(const GridPoint& start, const GridPoint& goal,
                                unsigned int width, unsigned int height,
                                const std::vector<GridPoint>& excludedAnchors,
                                const TransitionPredicate& transitionPassable) {
    static constexpr GridPoint directions[4] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    std::unordered_set<GridPoint, GridPointHash> excluded;
    excluded.reserve(excludedAnchors.size());
    excluded.insert(excludedAnchors.begin(), excludedAnchors.end());

    std::map<GridPoint, GridPoint> cameFrom;
    std::map<GridPoint, int> gScore;
    gScore[start] = 0;
    std::map<GridPoint, int> fScore;
    fScore[start] = std::abs(start.first - goal.first) +
                    std::abs(start.second - goal.second);
    using QueueNode = std::pair<int, GridPoint>;
    std::priority_queue<QueueNode, std::vector<QueueNode>,
                        std::greater<QueueNode>>
        openQueue;
    openQueue.push({fScore[start], start});
    while (!openQueue.empty()) {
        const GridPoint current = openQueue.top().second;
        const int currentF = openQueue.top().first;
        openQueue.pop();
        if (currentF > nodeFScore(fScore, current)) {
            continue;
        }
        if (current == goal) {
            std::vector<GridPoint> path;
            GridPoint cursor = current;
            while (cameFrom.find(cursor) != cameFrom.end()) {
                path.push_back(cursor);
                cursor = cameFrom[cursor];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        const auto [currentX, currentY] = current;
        for (const auto [deltaX, deltaY] : directions) {
            const int nextX = currentX + deltaX;
            const int nextY = currentY + deltaY;
            if (!inBounds(nextX, nextY, width, height)) {
                continue;
            }
            const GridPoint next = {nextX, nextY};
            if (next != start && excluded.contains(next)) {
                continue;
            }
            if (!transitionPassable(currentX, currentY, nextX, nextY)) {
                continue;
            }
            const int tentative = gScore[current] + 1;
            const auto previous = gScore.find(next);
            const int previousScore =
                previous == gScore.end() ? 1 << 30 : previous->second;
            if (tentative >= previousScore) {
                continue;
            }
            cameFrom[next] = current;
            gScore[next] = tentative;
            const int nextF = tentative + std::abs(nextX - goal.first) +
                              std::abs(nextY - goal.second);
            fScore[next] = nextF;
            openQueue.push({nextF, next});
        }
    }
    return {};
}

}  // namespace ludork::global::game_map_base_impl
