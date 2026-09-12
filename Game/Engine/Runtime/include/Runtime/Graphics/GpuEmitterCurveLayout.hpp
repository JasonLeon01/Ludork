#pragma once

#include <cstddef>

namespace ludork::runtime::graphics::emitter_curve_layout {

inline constexpr std::size_t SampleCount = 256;
inline constexpr std::size_t ChannelCount = 8;
inline constexpr std::size_t ComponentsPerTexel = 4;
inline constexpr std::size_t TextureRowCount =
    ChannelCount / ComponentsPerTexel;
inline constexpr std::size_t SampleValueCount = SampleCount * ChannelCount;

static_assert(SampleCount > 1);
static_assert(ChannelCount % ComponentsPerTexel == 0);

constexpr std::size_t sampleIndex(std::size_t channel, std::size_t sample) {
    return ((channel / ComponentsPerTexel) * SampleCount + sample) *
               ComponentsPerTexel +
           channel % ComponentsPerTexel;
}

}  // namespace ludork::runtime::graphics::emitter_curve_layout
