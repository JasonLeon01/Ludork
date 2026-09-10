#pragma once
#include <cstddef>
#include <utility>
namespace ludork::global::game_map_base_impl {
struct GridPointHash {
    std::size_t operator()(const std::pair<int, int>& value) const;
};
}  // namespace ludork::global::game_map_base_impl
