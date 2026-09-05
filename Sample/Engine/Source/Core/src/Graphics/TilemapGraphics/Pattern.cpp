#include "Pattern.hpp"

namespace ludork::engine::tilemap_graphics_impl {
namespace {
constexpr int kInnerFillerCell = 3;

constexpr std::array<std::array<int, 4>, 16> kBasePattern = {{
    {{1, 1, 1, 1}},
    {{10, 12, 10, 12}},
    {{4, 4, 10, 10}},
    {{10, 10, 10, 10}},
    {{4, 6, 4, 6}},
    {{7, 9, 7, 9}},
    {{4, 4, 4, 4}},
    {{7, 7, 7, 7}},
    {{6, 6, 12, 12}},
    {{12, 12, 12, 12}},
    {{5, 5, 11, 11}},
    {{11, 11, 11, 11}},
    {{6, 6, 6, 6}},
    {{9, 9, 9, 9}},
    {{5, 5, 5, 5}},
    {{8, 8, 8, 8}},
}};

constexpr std::array<std::array<int, 3>, 4> kQuadBits = {{
    {{kMaskTop, kMaskLeft, kMaskTopLeft}},
    {{kMaskTop, kMaskRight, kMaskTopRight}},
    {{kMaskBottom, kMaskLeft, kMaskBottomLeft}},
    {{kMaskBottom, kMaskRight, kMaskBottomRight}},
}};
}  // namespace

std::array<int, 4> composeCellPattern(int mask) {
    std::array<int, 4> result = kBasePattern[mask & 0x0F];
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
        const int firstOrthogonal = kQuadBits[quadrant][0];
        const int secondOrthogonal = kQuadBits[quadrant][1];
        const int diagonal = kQuadBits[quadrant][2];
        if ((mask & firstOrthogonal) && (mask & secondOrthogonal) &&
            !(mask & diagonal)) {
            result[quadrant] = kInnerFillerCell;
        }
    }
    return result;
}

}  // namespace ludork::engine::tilemap_graphics_impl
