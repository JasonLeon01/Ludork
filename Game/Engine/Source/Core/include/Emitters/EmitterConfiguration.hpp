#pragma once

#include <Emitters/EmitterTrack.hpp>
#include <string>
#include <vector>

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct EmitterConfiguration {
    BIND_PROPERTY()
    std::string name;
    BIND_PROPERTY()
    int simulationRate = 60;
    BIND_PROPERTY()
    int seed = 1;
    BIND_PROPERTY()
    std::vector<EmitterTrack> tracks;
};
