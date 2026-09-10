#pragma once

#include <array>

namespace ludork::engine::tilemap_graphics_impl {

inline constexpr int kMaskTop = 0x01;
inline constexpr int kMaskRight = 0x02;
inline constexpr int kMaskBottom = 0x04;
inline constexpr int kMaskLeft = 0x08;
inline constexpr int kMaskTopLeft = 0x10;
inline constexpr int kMaskTopRight = 0x20;
inline constexpr int kMaskBottomRight = 0x40;
inline constexpr int kMaskBottomLeft = 0x80;

std::array<int, 4> composeCellPattern(int mask);

}  // namespace ludork::engine::tilemap_graphics_impl
