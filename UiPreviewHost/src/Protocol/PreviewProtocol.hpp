#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace ludork::preview_host {

inline constexpr std::int64_t protocolVersion = 5;

const RuntimeValue* findValue(const RuntimeValue::Map& values,
                              const std::string& name);
const RuntimeValue::Map& requireMap(const RuntimeValue& value,
                                    const std::string& source);
const RuntimeValue::Array& requireArray(const RuntimeValue& value,
                                        const std::string& source);
const RuntimeValue& requireValue(const RuntimeValue::Map& values,
                                 const std::string& name,
                                 const std::string& source);
const std::string& requireString(const RuntimeValue& value,
                                 const std::string& source);
std::int64_t requireInteger(const RuntimeValue& value,
                            const std::string& source);
double requireNumber(const RuntimeValue& value, const std::string& source);
int requireInt32(const RuntimeValue& value, const std::string& source);

RuntimeValue::Map object(
    std::initializer_list<std::pair<const std::string, RuntimeValue>> values);
RuntimeValue number(float value);
RuntimeValue errorResponse(const std::string& message);

void configureProtocolStreams();
std::optional<std::string> readMessage();
void writeMessage(const RuntimeValue& value);

}  // namespace ludork::preview_host
