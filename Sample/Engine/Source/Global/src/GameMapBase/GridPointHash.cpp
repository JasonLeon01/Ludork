#include "GridPointHash.hpp"
#include <functional>
namespace ludork::global::game_map_base_impl {
std::size_t GridPointHash::operator()(const std::pair<int, int>& value) const {
    std::size_t xHash = std::hash<int>{}(value.first);
    std::size_t yHash = std::hash<int>{}(value.second);
    return xHash ^ (yHash + 0x9e3779b9 + (xHash << 6) + (xHash >> 2));
}
}  // namespace ludork::global::game_map_base_impl
