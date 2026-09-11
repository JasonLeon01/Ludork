#pragma once

#include <EngineRuntimeApi.hpp>

#include <memory>

class ControlBase;
class EmitterScheduler;

namespace ludork::engine {

LUDORK_ENGINE_API void collectUiEmitters(
    const std::shared_ptr<ControlBase>& root, EmitterScheduler& scheduler);

}  // namespace ludork::engine
