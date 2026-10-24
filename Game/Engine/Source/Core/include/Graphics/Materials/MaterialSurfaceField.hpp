#pragma once

#include <cstddef>

namespace ludork::engine::materials {

enum class MaterialSurfaceField : std::size_t {
    BaseColor,
    Alpha,
    Emission,
    LightingWeight,
    LightBlock,
    ReflectionStrength,
    VertexOffset,
    Count
};

constexpr std::size_t surfaceFieldIndex(MaterialSurfaceField field) {
    return static_cast<std::size_t>(field);
}

constexpr unsigned int surfaceFieldMask(MaterialSurfaceField field) {
    return 1u << surfaceFieldIndex(field);
}

inline constexpr std::size_t MaterialSurfaceFieldCount =
    surfaceFieldIndex(MaterialSurfaceField::Count);
inline constexpr unsigned int AllMaterialSurfaceFields =
    (1u << MaterialSurfaceFieldCount) - 1;

namespace surface_field_name {
inline constexpr char BaseColor[] = "BaseColor";
inline constexpr char Alpha[] = "Alpha";
inline constexpr char Emission[] = "Emission";
inline constexpr char LightingWeight[] = "LightingWeight";
inline constexpr char LightBlock[] = "LightBlock";
inline constexpr char ReflectionStrength[] = "ReflectionStrength";
inline constexpr char VertexOffset[] = "VertexOffset";
}  // namespace surface_field_name

}  // namespace ludork::engine::materials
