#include "EmitterConfigurationCompiler.hpp"
#include "EmitterCurveChannels.hpp"

#include <Curve.hpp>
#include <Runtime/Graphics/EmitterTrackParameters.hpp>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::engine::emitters {
namespace {

void requireRange(int value, int minimum, int maximum, const char* name) {
    if (value < minimum || value > maximum) {
        throw std::invalid_argument(std::string("particle.") + name +
                                    " is outside its supported range");
    }
}

void validateTrackParameters(
    const ludork::runtime::graphics::EmitterTrackParameters& track) {
    requireRange(track.scaleMode, 0, 2, "scaleMode");
    requireRange(track.shape, 0, 4, "shape");
    requireRange(track.capacity, 1, 1000000, "capacity");
    requireRange(track.count, 0, track.capacity, "count");
    requireRange(track.columns, 1, 4096, "columns");
    requireRange(track.rows, 1, 4096, "rows");
    requireRange(track.frameCount, 1, track.columns * track.rows, "frameCount");

    for (float value : {track.delay,
                        track.duration,
                        track.rate,
                        track.distanceRate,
                        track.extent.x,
                        track.extent.y,
                        track.radius,
                        track.innerRadius,
                        track.direction,
                        track.spread,
                        track.lifetime.x,
                        track.lifetime.y,
                        track.speed.x,
                        track.speed.y,
                        track.sizeMin.x,
                        track.sizeMin.y,
                        track.sizeMax.x,
                        track.sizeMax.y,
                        track.rotation.x,
                        track.rotation.y,
                        track.angularVelocity.x,
                        track.angularVelocity.y,
                        track.gravity.x,
                        track.gravity.y,
                        track.radialAcceleration,
                        track.tangentialAcceleration,
                        track.damping,
                        track.frameRate,
                        track.offset.x,
                        track.offset.y,
                        track.rotationOffset,
                        track.scale.x,
                        track.scale.y}) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "Particle track values must be finite: " + track.name);
        }
    }

    for (std::size_t index = 0; index < track.colourMin.size(); ++index) {
        if (!std::isfinite(track.colourMin[index]) ||
            !std::isfinite(track.colourMax[index]) ||
            track.colourMin[index] < 0 || track.colourMax[index] > 1 ||
            track.colourMin[index] > track.colourMax[index]) {
            throw std::invalid_argument("Invalid particle colour range: " +
                                        track.name);
        }
    }

    const sf::IntRect& rect = track.textureRect;
    if (rect.position.x < 0 || rect.position.y < 0 || rect.size.x < 0 ||
        rect.size.y < 0 || ((rect.size.x == 0) != (rect.size.y == 0))) {
        throw std::invalid_argument("Invalid particle textureRect: " +
                                    track.name);
    }

    if (track.duration <= 0 || track.delay < 0 || track.rate < 0 ||
        track.distanceRate < 0 || track.damping < 0 || track.radius < 0 ||
        track.innerRadius < 0 ||
        (track.shape == 4 && track.innerRadius > track.radius) ||
        track.lifetime.x <= 0 || track.lifetime.y < track.lifetime.x ||
        track.speed.x > track.speed.y || track.sizeMin.x < 0 ||
        track.sizeMin.y < 0 || track.sizeMax.x < track.sizeMin.x ||
        track.sizeMax.y < track.sizeMin.y || track.frameRate < 0 ||
        track.rotation.x > track.rotation.y ||
        track.angularVelocity.x > track.angularVelocity.y ||
        track.extent.x < 0 || track.extent.y < 0 || track.spread < 0) {
        throw std::invalid_argument("Invalid particle track range: " +
                                    track.name);
    }

    for (const ludork::runtime::graphics::EmitterBurst& burst : track.bursts) {
        requireRange(burst.count, 0, 1000000, "burst.count");
        requireRange(burst.cycles, 1, 10000, "burst.cycles");
        if (!std::isfinite(burst.time) || !std::isfinite(burst.interval) ||
            burst.time < 0 || burst.time >= track.duration ||
            burst.interval < 0 || (burst.cycles > 1 && burst.interval <= 0)) {
            throw std::invalid_argument("Invalid particle burst: " +
                                        track.name);
        }
    }
}

void validateCurve(const Curve::CurveData& curve, const char* channel) {
    if (!std::isfinite(curve.defaultValue)) {
        throw std::invalid_argument(
            std::string("Particle curve default must be finite: ") + channel);
    }
    for (const CurveKey& key : curve.keys) {
        if (!std::isfinite(key.time) || !std::isfinite(key.value) ||
            !std::isfinite(key.arriveTangent) ||
            !std::isfinite(key.leaveTangent)) {
            throw std::invalid_argument(
                std::string("Particle curve values must be finite: ") +
                channel);
        }
        if (key.time < 0 || key.time > 1) {
            throw std::invalid_argument(
                std::string(
                    "Particle curve time must be normalized to [0,1]: ") +
                channel);
        }
    }
}

}  // namespace

ludork::runtime::graphics::GpuEmitterConfiguration compileEmitterConfiguration(
    const EmitterConfiguration& configuration) {
    requireRange(configuration.simulationRate, 1, 240, "simulationRate");
    requireRange(configuration.seed, 0, 16777215, "seed");

    ludork::runtime::graphics::GpuEmitterConfiguration result;
    result.simulationRate = configuration.simulationRate;
    result.seed = configuration.seed;
    result.tracks.reserve(configuration.tracks.size());
    std::set<std::string> names;
    for (const EmitterTrack& track : configuration.tracks) {
        if (!names.insert(track.name).second) {
            throw std::invalid_argument("Duplicate particle track: " +
                                        track.name);
        }
        validateTrackParameters(track);
        ludork::runtime::graphics::GpuEmitterConfiguration::Track compiled;
        static_cast<ludork::runtime::graphics::EmitterTrackParameters&>(
            compiled) =
            static_cast<
                const ludork::runtime::graphics::EmitterTrackParameters&>(
                track);
        for (std::size_t channel = 0; channel < emitterCurveChannels.size();
             ++channel) {
            const auto& [name, member] = emitterCurveChannels[channel];
            const Curve::CurveData& curveData = track.curves.*member;
            validateCurve(curveData, name);
            Curve curve(curveData);
            for (std::size_t sample = 0; sample < 256; ++sample) {
                const float value =
                    curve.evaluate(static_cast<float>(sample) / 255);
                if (!std::isfinite(value)) {
                    throw std::invalid_argument(
                        std::string("Particle curve sample must be finite: ") +
                        name);
                }
                compiled.curveSamples[(channel / 4) * 256 * 4 + sample * 4 +
                                      channel % 4] = value;
            }
        }
        result.tracks.push_back(std::move(compiled));
    }
    return result;
}

}  // namespace ludork::engine::emitters
