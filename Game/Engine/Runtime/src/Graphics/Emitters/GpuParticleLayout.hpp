#pragma once

#include <array>
#include <cstddef>

namespace ludork::runtime::graphics::gpu_particle_layout {

inline constexpr std::array<const char*, 6> StateAttributeNames{
    "a0", "a1", "a2", "a3", "a4", "a5"};
inline constexpr std::array<const char*, StateAttributeNames.size()>
    FeedbackNames{"s0", "s1", "s2", "s3", "s4", "s5"};
inline constexpr unsigned int ComponentsPerAttribute = 4;
inline constexpr unsigned int StateAttributeCount = StateAttributeNames.size();
inline constexpr unsigned int StateFloatCount =
    StateAttributeCount * ComponentsPerAttribute;
inline constexpr std::size_t StateStrideBytes = StateFloatCount * sizeof(float);
inline constexpr std::size_t AgeOffsetBytes =
    ComponentsPerAttribute * sizeof(float);
inline constexpr unsigned int CornerAttributeIndex = StateAttributeCount;
inline constexpr char CornerAttributeName[] = "corner";
inline constexpr int QuadVertexCount = 6;
inline constexpr int QuadComponentsPerVertex = 2;

}  // namespace ludork::runtime::graphics::gpu_particle_layout
