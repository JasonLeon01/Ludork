#pragma once

#include <cstddef>
#include <utility>

namespace ludork::global::game_map_base_impl {

int pageCoordinate(int value, int pageSize);
int pageOffset(int value, int pageSize);
std::pair<int, int> pageKey(int x, int y, int pageSize);
std::size_t pageCellIndex(int x, int y, int pageSize);

}  // namespace ludork::global::game_map_base_impl
