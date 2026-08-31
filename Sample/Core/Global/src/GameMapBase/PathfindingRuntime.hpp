#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace ludork::global::game_map_base_impl {

using GridPoint = std::pair<int, int>;
using TransitionPredicate =
    std::function<bool(int fromX, int fromY, int toX, int toY)>;

std::vector<GridPoint> findPath(const GridPoint& start, const GridPoint& goal,
                                unsigned int width, unsigned int height,
                                const std::vector<GridPoint>& excludedAnchors,
                                const TransitionPredicate& transitionPassable);

}  // namespace ludork::global::game_map_base_impl
