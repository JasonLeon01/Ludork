#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <cstdint>
#include <string>

namespace ludork::runtime::value_reader {

LUDORK_RUNTIME_API const RuntimeValue* findValue(
    const RuntimeValue::Map& values, const std::string& name);

LUDORK_RUNTIME_API const RuntimeValue& requireValue(
    const RuntimeValue::Map& values, const std::string& name,
    const std::string& source);

LUDORK_RUNTIME_API const RuntimeValue::Map& requireMap(
    const RuntimeValue& value, const std::string& source);

LUDORK_RUNTIME_API const RuntimeValue::Array& requireArray(
    const RuntimeValue& value, const std::string& source);

LUDORK_RUNTIME_API const std::string& requireString(const RuntimeValue& value,
                                                    const std::string& source);

LUDORK_RUNTIME_API bool requireBool(const RuntimeValue& value,
                                    const std::string& source);

LUDORK_RUNTIME_API double requireNumber(const RuntimeValue& value,
                                        const std::string& source);

LUDORK_RUNTIME_API float requireFloat(const RuntimeValue& value,
                                      const std::string& source);

LUDORK_RUNTIME_API std::int64_t requireInteger(const RuntimeValue& value,
                                               const std::string& source);

LUDORK_RUNTIME_API int requireInt(const RuntimeValue& value,
                                  const std::string& source);

LUDORK_RUNTIME_API unsigned int requireUnsigned(const RuntimeValue& value,
                                                const std::string& source);

}  // namespace ludork::runtime::value_reader
