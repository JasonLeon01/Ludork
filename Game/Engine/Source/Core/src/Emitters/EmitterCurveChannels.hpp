#pragma once

#include <Emitters/EmitterCurves.hpp>
#include <Runtime/Graphics/GpuEmitterCurveLayout.hpp>
#include <array>
#include <utility>

namespace ludork::engine::emitters {

inline constexpr std::array<
    std::pair<const char*, Curve::CurveData EmitterCurves::*>,
    ludork::runtime::graphics::emitter_curve_layout::ChannelCount>
    emitterCurveChannels{{{"speed", &EmitterCurves::speed},
                          {"sizeX", &EmitterCurves::sizeX},
                          {"sizeY", &EmitterCurves::sizeY},
                          {"rotation", &EmitterCurves::rotation},
                          {"red", &EmitterCurves::red},
                          {"green", &EmitterCurves::green},
                          {"blue", &EmitterCurves::blue},
                          {"alpha", &EmitterCurves::alpha}}};

}  // namespace ludork::engine::emitters
