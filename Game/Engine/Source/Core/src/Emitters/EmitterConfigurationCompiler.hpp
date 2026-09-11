#pragma once

#include <Emitters/EmitterConfiguration.hpp>
#include <Runtime/Graphics/GpuEmitterConfiguration.hpp>

namespace ludork::engine::emitters {

ludork::runtime::graphics::GpuEmitterConfiguration compileEmitterConfiguration(
    const EmitterConfiguration& configuration);

}  // namespace ludork::engine::emitters
