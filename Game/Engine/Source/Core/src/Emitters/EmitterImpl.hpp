#pragma once

#include <Emitters/Emitter.hpp>
#include <Runtime/Graphics/GpuEmitterBackend.hpp>

struct Emitter::Impl {
    EmitterConfiguration configuration;
    ludork::runtime::graphics::GpuEmitterBackend backend;
};
