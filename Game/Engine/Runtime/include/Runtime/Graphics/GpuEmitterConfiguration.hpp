#pragma once

#include <Runtime/Graphics/EmitterTrackParameters.hpp>
#include <array>
#include <vector>

namespace ludork::runtime::graphics {

struct GpuEmitterConfiguration {
    struct Track : EmitterTrackParameters {
        std::array<float, 256 * 8> curveSamples{};
    };
    int simulationRate = 60;
    int seed = 1;
    std::vector<Track> tracks;
};

}  // namespace ludork::runtime::graphics
