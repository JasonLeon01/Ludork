#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace ludork::preview_host {

inline constexpr std::int64_t protocolVersion = 5;

RuntimeValue::Map object(
    std::initializer_list<std::pair<const std::string, RuntimeValue>> values);
RuntimeValue number(float value);
RuntimeValue errorResponse(const std::string& message);

void configureProtocolStreams();
std::optional<std::string> readMessage();
void writeMessage(const RuntimeValue& value);

}  // namespace ludork::preview_host
