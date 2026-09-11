#pragma once

#include <Emitters/EmitterConfiguration.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeData.hpp>
#include <string>

namespace ludork::engine::emitters {

LUDORK_ENGINE_API EmitterConfiguration
loadEmitterConfiguration(const std::string& resourceKey);
LUDORK_ENGINE_API EmitterConfiguration
parseEmitterConfiguration(const RuntimeData& data);

}  // namespace ludork::engine::emitters
