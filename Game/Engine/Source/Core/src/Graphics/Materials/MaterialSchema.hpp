#pragma once

#include <Runtime/RuntimeData.hpp>
#include <array>
#include <string_view>

namespace ludork::engine::materials::schema {

namespace node_type {
inline constexpr char Surface[] = "Surface";
inline constexpr char SurfaceInput[] = "SurfaceInput";
inline constexpr char Constant[] = "Constant";
inline constexpr char Parameter[] = "Parameter";
inline constexpr char Time[] = "Time";
inline constexpr char LocalUV[] = "LocalUV";
inline constexpr char LocalPosition[] = "LocalPosition";
inline constexpr char WorldPosition[] = "WorldPosition";
inline constexpr char FrameSize[] = "FrameSize";
inline constexpr char SampleSprite[] = "SampleSprite";
inline constexpr char SampleTexture[] = "SampleTexture";
inline constexpr char Compose[] = "Compose";
inline constexpr char Swizzle[] = "Swizzle";
inline constexpr char HueShift[] = "HueShift";
inline constexpr char Abs[] = "Abs";
inline constexpr char Floor[] = "Floor";
inline constexpr char Fract[] = "Fract";
inline constexpr char Sin[] = "Sin";
inline constexpr char Clamp[] = "Clamp";
inline constexpr char Smoothstep[] = "Smoothstep";
inline constexpr char Lerp[] = "Lerp";
inline constexpr char Divide[] = "Divide";
inline constexpr char Dot[] = "Dot";
inline constexpr char Multiply[] = "Multiply";
inline constexpr char Add[] = "Add";
inline constexpr char Subtract[] = "Subtract";
inline constexpr char Min[] = "Min";
inline constexpr char Max[] = "Max";
}  // namespace node_type

namespace value_type {
inline constexpr char Float[] = "float";
inline constexpr char Vec2[] = "vec2";
inline constexpr char Vec3[] = "vec3";
inline constexpr char Vec4[] = "vec4";
inline constexpr char Color[] = "color";
inline constexpr char Boolean[] = "bool";
inline constexpr char Texture[] = "texture";
inline constexpr char Numeric[] = "numeric";
inline constexpr char Integer[] = "integer";
inline constexpr char String[] = "string";
inline constexpr char Parameter[] = "parameter";
}  // namespace value_type

namespace category {
inline constexpr char Input[] = "Input";
inline constexpr char Output[] = "Output";
inline constexpr char Texture[] = "Texture";
inline constexpr char Math[] = "Math";
}  // namespace category

namespace port_name {
inline constexpr char Value[] = "Value";
inline constexpr char Color[] = "Color";
inline constexpr char Alpha[] = "Alpha";
inline constexpr char RGBA[] = "RGBA";
inline constexpr char UV[] = "UV";
inline constexpr char Texture[] = "Texture";
inline constexpr char A[] = "A";
inline constexpr char B[] = "B";
inline constexpr char Min[] = "Min";
inline constexpr char Max[] = "Max";
}  // namespace port_name

inline constexpr std::array<int, 3> composeWidths{2, 3, 4};
inline constexpr std::string_view positionChannels = "xyzw";
inline constexpr std::string_view colorChannels = "rgba";

RuntimeData::Array nodeDefinitions();
RuntimeData::Array renderingFields();
RuntimeData defaultDocument();
int valueWidth(const RuntimeData& value);
int typeWidth(const std::string& type);
void validateParameterValue(const std::string& type, const RuntimeData& value,
                            const std::string& source);

}  // namespace ludork::engine::materials::schema
