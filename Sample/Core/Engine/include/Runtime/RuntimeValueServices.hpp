#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <vector>

namespace ludork::engine::runtime_services {

LUDORK_ENGINE_API RuntimeValue
firstResult(const std::vector<RuntimeValue>& values);

LUDORK_ENGINE_API RuntimeValue invokeFirst(
    const std::string& operation, std::vector<RuntimeValue> arguments = {});

LUDORK_ENGINE_API bool invokeBool(const std::string& operation,
                                  std::vector<RuntimeValue> arguments = {});

LUDORK_ENGINE_API std::string invokeString(
    const std::string& operation, std::vector<RuntimeValue> arguments = {});

}  // namespace ludork::engine::runtime_services
