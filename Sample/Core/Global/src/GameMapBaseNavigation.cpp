#include "GameMapBase.hpp"

#include <Gameplay/Actor.hpp>
#include <Runtime/EngineState.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <unordered_set>
#include <utility>

using Node = std::pair<int, IntPair>;

namespace {

bool inBounds(int x, int y, int width, int height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}

int getNodeFScore(const std::map<IntPair, int>& fScore, const IntPair& node) {
    const auto iterator = fScore.find(node);
    if (iterator == fScore.end()) {
        return 1 << 30;
    }
    return iterator->second;
}

}  // namespace

PathResult GameMapBase::findPathExt(
    const sf::Vector2i& start, const sf::Vector2i& goal,
    const sf::Vector2u& size, Actor& movingActor,
    const std::vector<sf::Vector2i>& excludedAnchors) {
    ensurePassabilityCache();
    refreshActorOccupancyCache();
    int sx = start.x;
    int sy = start.y;
    int gx = goal.x;
    int gy = goal.y;
    unsigned int width = size.x;
    unsigned int height = size.y;
    IntPair dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    IntPair start_t = {sx, sy};
    IntPair goal_t = {gx, gy};
    std::unordered_set<IntPair, IntPairHash> excludedAnchorSet;
    excludedAnchorSet.reserve(excludedAnchors.size());
    for (const sf::Vector2i& anchor : excludedAnchors) {
        excludedAnchorSet.emplace(anchor.x, anchor.y);
    }
    PathResult result;
    if (start_t == goal_t) {
        result.route.emplace_back(sx, sy);
        return result;
    }
    std::map<IntPair, IntPair> cameFrom;
    std::map<IntPair, int> gScore;
    gScore[start_t] = 0;
    std::map<IntPair, int> fScore;
    fScore[start_t] = std::abs(sx - gx) + std::abs(sy - gy);
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openQueue;
    openQueue.push({fScore[start_t], start_t});
    while (!openQueue.empty()) {
        IntPair current = openQueue.top().second;
        int currentF = openQueue.top().first;
        openQueue.pop();
        if (currentF > getNodeFScore(fScore, current)) {
            continue;
        }
        if (current == goal_t) {
            std::vector<IntPair> pathPosition;
            auto c = current;
            while (cameFrom.find(c) != cameFrom.end()) {
                pathPosition.push_back(c);
                c = cameFrom[c];
            }
            std::reverse(pathPosition.begin(), pathPosition.end());
            result.offsets.reserve(pathPosition.size());
            result.points.reserve(pathPosition.size());
            result.route.reserve(pathPosition.size() + 1);
            result.route.emplace_back(sx, sy);
            int px = sx;
            int py = sy;
            for (auto& [x, y] : pathPosition) {
                result.offsets.emplace_back(x - px, y - py);
                result.points.emplace_back(x, y);
                result.route.emplace_back(x, y);
                px = x;
                py = y;
            }
            return result;
        }
        auto [cx, cy] = current;
        for (auto& [dx, dy] : dirs) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (!inBounds(nx, ny, width, height)) {
                continue;
            }
            const IntPair nt = {nx, ny};
            if (nt != start_t &&
                excludedAnchorSet.find(nt) != excludedAnchorSet.end()) {
                continue;
            }
            if (!transitionPassableForActor(cx, cy, nx, ny, sx, sy, gx, gy,
                                            width, height, movingActor)) {
                continue;
            }
            int tentative = gScore[current] + 1;
            int prevG = (gScore.count(nt)) ? gScore[nt] : (1 << 30);
            if (tentative < prevG) {
                cameFrom[nt] = current;
                gScore[nt] = tentative;
                int nextF = tentative + std::abs(nx - gx) + std::abs(ny - gy);
                fScore[nt] = nextF;
                openQueue.push({nextF, nt});
            }
        }
    }
    return result;
}

bool GameMapBase::transitionPassableForActor(int fromX, int fromY, int x, int y,
                                             int sx, int sy, int gx, int gy,
                                             unsigned int width,
                                             unsigned int height,
                                             const Actor& movingActor) {
    const std::vector<sf::Vector2i> cells =
        movingActor.getOccupiedMapCellsAtMapPosition({x, y});
    if (cells.empty()) {
        return passable(x, y, sx, sy, gx, gy) &&
               directionPassableForActor({fromX, fromY}, {x, y}, cells,
                                         movingActor);
    }
    for (const sf::Vector2i& cell : cells) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= static_cast<int>(width) ||
            cell.y >= static_cast<int>(height)) {
            return false;
        }
        if (!passableForActor(cell.x, cell.y, sx, sy, gx, gy, &movingActor)) {
            return false;
        }
    }
    return directionPassableForActor({fromX, fromY}, {x, y}, cells,
                                     movingActor);
}

bool GameMapBase::directionPassableForActor(
    const sf::Vector2i& fromPosition, const sf::Vector2i& toPosition,
    const std::vector<sf::Vector2i>& toCells, const Actor& movingActor) const {
    const sf::Vector2i delta = toPosition - fromPosition;
    int direction = -1;
    if (delta == sf::Vector2i(0, 1)) {
        direction = Direction.at("DOWN");
    } else if (delta == sf::Vector2i(0, -1)) {
        direction = Direction.at("UP");
    } else if (delta == sf::Vector2i(1, 0)) {
        direction = Direction.at("RIGHT");
    } else if (delta == sf::Vector2i(-1, 0)) {
        direction = Direction.at("LEFT");
    }
    if (direction < 0) {
        return true;
    }
    const std::vector<sf::Vector2i> fromCells =
        movingActor.getOccupiedMapCellsAtMapPosition(fromPosition);
    std::unordered_set<IntPair, IntPairHash> fromCellSet;
    fromCellSet.reserve(fromCells.size());
    for (const sf::Vector2i& cell : fromCells) {
        fromCellSet.emplace(cell.x, cell.y);
    }
    for (const sf::Vector2i& cell : toCells) {
        if (fromCellSet.contains({cell.x, cell.y})) {
            continue;
        }
        const sf::Vector2i previous(cell.x - delta.x, cell.y - delta.y);
        if (!isDirectionPassable(previous, cell, direction)) {
            return false;
        }
    }
    return true;
}

bool GameMapBase::isDirectionPassable(const sf::Vector2i& fromPosition,
                                      const sf::Vector2i& toPosition,
                                      int direction) const {
    if (sparseWorldSize_.has_value()) {
        return sparseDirectionPassabilityQuery_(fromPosition, toPosition,
                                                direction);
    }
    const int opposite = oppositeDirection(direction);
    bool fromFound = false;
    bool toFound = false;
    for (const std::string& layerName : getTopFirstLayerNames()) {
        const std::shared_ptr<TileLayer> layer = tilemap_->getLayer(layerName);
        if (layer == nullptr || !layer->getVisible()) {
            continue;
        }
        if (!fromFound && layer->get(fromPosition).has_value()) {
            if (!layer->isDirectionPassable(fromPosition, direction)) {
                return false;
            }
            fromFound = true;
        }
        if (!toFound && layer->get(toPosition).has_value()) {
            if (!layer->isDirectionPassable(toPosition, opposite)) {
                return false;
            }
            toFound = true;
        }
        if (fromFound && toFound) {
            break;
        }
    }
    return true;
}
