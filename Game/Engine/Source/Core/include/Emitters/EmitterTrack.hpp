#pragma once

#include <Emitters/EmitterCurves.hpp>
#include <Runtime/Graphics/EmitterTrackParameters.hpp>

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct EmitterTrack : ludork::runtime::graphics::EmitterTrackParameters {
    BIND_PROPERTY()
    EmitterCurves curves;
};
