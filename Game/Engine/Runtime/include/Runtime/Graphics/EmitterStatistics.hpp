#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace ludork::runtime::graphics {

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LUDORK_RUNTIME_API EmitterStatistics {
    BIND_PROPERTY()
    int capacity = 0;

    BIND_PROPERTY()
    double time = 0;

    BIND_PROPERTY()
    std::optional<std::string> renderer;

    BIND_PROPERTY()
    std::optional<std::int64_t> aliveCount;

    BIND_PROPERTY()
    std::optional<double> sampledTime;

    BIND_PROPERTY()
    std::optional<double> gpuSimulationMs;

    BIND_PROPERTY()
    std::optional<double> gpuDrawMs;

    BIND_PROPERTY()
    std::optional<std::int64_t> simulationSample;

    BIND_PROPERTY()
    std::optional<std::int64_t> drawSample;
};

}  // namespace ludork::runtime::graphics
