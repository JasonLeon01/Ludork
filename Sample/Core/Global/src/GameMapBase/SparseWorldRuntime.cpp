#include "SparseWorldRuntime.hpp"

#include <cstdint>

namespace ludork::global::game_map_base_impl {

bool rectContains(const sf::IntRect& rect, const sf::Vector2i& position) {
    const std::int64_t right = static_cast<std::int64_t>(rect.position.x) +
                               static_cast<std::int64_t>(rect.size.x);
    const std::int64_t bottom = static_cast<std::int64_t>(rect.position.y) +
                                static_cast<std::int64_t>(rect.size.y);
    return position.x >= rect.position.x && position.y >= rect.position.y &&
           static_cast<std::int64_t>(position.x) < right &&
           static_cast<std::int64_t>(position.y) < bottom;
}

bool rectsIntersect(const sf::IntRect& left, const sf::IntRect& right) {
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

}  // namespace ludork::global::game_map_base_impl
