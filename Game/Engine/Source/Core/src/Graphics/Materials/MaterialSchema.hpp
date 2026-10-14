#pragma once

#include <Runtime/RuntimeData.hpp>
#include <array>
#include <string_view>

namespace ludork::engine::materials::schema {

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
