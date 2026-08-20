#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <cstdint>
#include <string>

namespace ludork::engine::runtime_value_reader {

LUDORK_ENGINE_API const RuntimeValue* findValue(const RuntimeValue::Map& values,
                                                const std::string& name);

LUDORK_ENGINE_API const RuntimeValue& requireValue(
    const RuntimeValue::Map& values, const std::string& name,
    const std::string& source);

LUDORK_ENGINE_API const RuntimeValue::Map& requireMap(
    const RuntimeValue& value, const std::string& source);

LUDORK_ENGINE_API const RuntimeValue::Array& requireArray(
    const RuntimeValue& value, const std::string& source);

LUDORK_ENGINE_API const std::string& requireString(const RuntimeValue& value,
                                                   const std::string& source);

LUDORK_ENGINE_API bool requireBool(const RuntimeValue& value,
                                   const std::string& source);

LUDORK_ENGINE_API double requireNumber(const RuntimeValue& value,
                                       const std::string& source);

LUDORK_ENGINE_API float requireFloat(const RuntimeValue& value,
                                     const std::string& source);

LUDORK_ENGINE_API std::int64_t requireInteger(const RuntimeValue& value,
                                              const std::string& source);

LUDORK_ENGINE_API int requireInt(const RuntimeValue& value,
                                 const std::string& source);

LUDORK_ENGINE_API unsigned int requireUnsigned(const RuntimeValue& value,
                                               const std::string& source);

}  // namespace ludork::engine::runtime_value_reader
