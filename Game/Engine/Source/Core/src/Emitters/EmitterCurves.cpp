#include <Emitters/EmitterCurves.hpp>
#include "EmitterCurveChannels.hpp"

EmitterCurves::EmitterCurves() {
    for (const auto& channel : ludork::engine::emitters::emitterCurveChannels) {
        (this->*channel.second).defaultValue =
            channel.second == &EmitterCurves::rotation ? 0 : 1;
    }
}
