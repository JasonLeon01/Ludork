#pragma once

#include <Runtime/RuntimeData.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace ludork::preview_host {

inline constexpr std::int64_t protocolVersion = 7;

RuntimeData::Map object(
    std::initializer_list<std::pair<const std::string, RuntimeData>> values);
RuntimeData number(float value);
RuntimeData errorResponse(const std::string& message);

void configureProtocolStreams();
std::optional<std::string> readMessage();
void writeMessage(const RuntimeData& value);

}  // namespace ludork::preview_host
