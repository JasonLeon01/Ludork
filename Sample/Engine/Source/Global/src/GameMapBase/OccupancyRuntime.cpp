#include "OccupancyRuntime.hpp"

namespace ludork::global::game_map_base_impl {

int pageCoordinate(int value, int pageSize) {
    return value >= 0 ? value / pageSize : -1 - (-(value + 1) / pageSize);
}

int pageOffset(int value, int pageSize) {
    return value - pageCoordinate(value, pageSize) * pageSize;
}

std::pair<int, int> pageKey(int x, int y, int pageSize) {
    return {pageCoordinate(x, pageSize), pageCoordinate(y, pageSize)};
}

std::size_t pageCellIndex(int x, int y, int pageSize) {
    return static_cast<std::size_t>(pageOffset(y, pageSize) * pageSize +
                                    pageOffset(x, pageSize));
}

}  // namespace ludork::global::game_map_base_impl
