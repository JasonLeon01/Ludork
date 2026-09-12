#include <Emitters/EmitterResource.hpp>
#include "EmitterCurveChannels.hpp"
#include <Curve.hpp>
#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ludork::engine::emitters {
namespace {
float number(const RuntimeData::Map& map, const std::string& key,
             float fallback) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(map, key);
    return value == nullptr ? fallback
                            : ludork::runtime::value_reader::requireFloat(
                                  *value, "particle." + key);
}
int integer(const RuntimeData::Map& map, const std::string& key, int fallback) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(map, key);
    return value == nullptr ? fallback
                            : ludork::runtime::value_reader::requireInt(
                                  *value, "particle." + key);
}
bool boolean(const RuntimeData::Map& map, const std::string& key,
             bool fallback) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(map, key);
    return value == nullptr ? fallback
                            : ludork::runtime::value_reader::requireBool(
                                  *value, "particle." + key);
}
std::string string(const RuntimeData::Map& map, const std::string& key,
                   const std::string& fallback) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(map, key);
    return value == nullptr ? fallback
                            : ludork::runtime::value_reader::requireString(
                                  *value, "particle." + key);
}
int choice(const RuntimeData::Map& map, const std::string& key,
           std::initializer_list<const char*> choices, int fallback) {
    const RuntimeData* input =
        ludork::runtime::value_reader::findValue(map, key);
    if (input == nullptr) {
        return fallback;
    }
    const std::string value =
        ludork::runtime::value_reader::requireString(*input, "particle." + key);
    int index = 0;
    for (const char* candidate : choices) {
        if (value == candidate) {
            return index;
        }
        ++index;
    }
    throw std::invalid_argument("Unsupported particle." + key + ": " + value);
}
template <std::size_t N>
std::array<float, N> vector(const RuntimeData::Map& map, const std::string& key,
                            const std::array<float, N>& fallback) {
    const RuntimeData* value =
        ludork::runtime::value_reader::findValue(map, key);
    if (value == nullptr) {
        return fallback;
    }
    const RuntimeData::Array& array =
        ludork::runtime::value_reader::requireArray(*value, "particle." + key);
    if (array.size() != N) {
        throw std::invalid_argument("Invalid particle vector: " + key);
    }
    std::array<float, N> result;
    for (std::size_t index = 0; index < N; ++index) {
        result[index] = ludork::runtime::value_reader::requireFloat(
            array[index], "particle." + key);
    }
    return result;
}
sf::Vector2f vector2(const RuntimeData::Map& map, const std::string& key,
                     sf::Vector2f fallback) {
    const auto result = vector<2>(map, key, {fallback.x, fallback.y});
    return {result[0], result[1]};
}
std::array<float, 4> colour(const RuntimeData::Map& map, const std::string& key,
                            const std::array<float, 4>& fallback) {
    if (ludork::runtime::value_reader::findValue(map, key) == nullptr) {
        return fallback;
    }
    std::array<float, 4> result = vector<4>(map, key, fallback);
    for (float& component : result) {
        component /= 255;
    }
    return result;
}
Curve::CurveData curve(const RuntimeData& input, float fallback) {
    RuntimeData data = input;
    if (const std::string* key = data.getIf<std::string>()) {
        data = getJSONData("Data/Curves/" + *key + ".json");
    }
    const RuntimeData::Map& map =
        ludork::runtime::value_reader::requireMap(data, "particle.curve");
    Curve::CurveData result;
    result.defaultValue = number(map, "defaultValue", fallback);
    result.preInfinity = string(map, "preInfinity", result.preInfinity);
    result.postInfinity = string(map, "postInfinity", result.postInfinity);
    if (const RuntimeData* keys =
            ludork::runtime::value_reader::findValue(map, "keys")) {
        for (const RuntimeData& entry :
             ludork::runtime::value_reader::requireArray(
                 *keys, "particle.curve.keys")) {
            const RuntimeData::Map& key =
                ludork::runtime::value_reader::requireMap(entry,
                                                          "particle.curve.key");
            CurveKey item;
            item.time = number(key, "time", item.time);
            item.value = number(key, "value", fallback);
            item.interpolation =
                string(key, "interpolation", item.interpolation);
            item.arriveTangent =
                number(key, "arriveTangent", item.arriveTangent);
            item.leaveTangent = number(key, "leaveTangent", item.leaveTangent);
            result.keys.push_back(item);
        }
    }
    return result;
}
}  // namespace

EmitterConfiguration loadEmitterConfiguration(const std::string& resourceKey) {
    if (resourceKey.empty() || resourceKey.front() == '/' ||
        resourceKey.find("..") != std::string::npos ||
        resourceKey.find('\\') != std::string::npos) {
        throw std::invalid_argument("Invalid particle resource key: " +
                                    resourceKey);
    }
    return parseEmitterConfiguration(
        getJSONData("Data/Particles/" + resourceKey + ".json"));
}

EmitterConfiguration parseEmitterConfiguration(const RuntimeData& data) {
    const RuntimeData::Map& root =
        ludork::runtime::value_reader::requireMap(data, "particle");
    if (string(root, "type", "") != "particle") {
        throw std::invalid_argument("Emitter resource type must be particle");
    }
    EmitterConfiguration result;
    result.name = string(root, "name", result.name);
    result.simulationRate =
        integer(root, "simulationRate", result.simulationRate);
    result.seed = integer(root, "seed", result.seed);
    const RuntimeData* tracks =
        ludork::runtime::value_reader::findValue(root, "tracks");
    if (tracks == nullptr) {
        return result;
    }
    for (const RuntimeData& entry : ludork::runtime::value_reader::requireArray(
             *tracks, "particle.tracks")) {
        const RuntimeData::Map& map =
            ludork::runtime::value_reader::requireMap(entry, "particle.track");
        EmitterTrack track;
        track.name = string(map, "name", track.name);
        track.texture = string(map, "texture", track.texture);
        track.enabled = boolean(map, "enabled", track.enabled);
        track.resident = choice(map, "mode", {"emission", "resident"},
                                track.resident ? 1 : 0) == 1;
        track.world =
            choice(map, "space", {"local", "world"}, track.world ? 1 : 0) == 1;
        track.additive =
            choice(map, "blend", {"alpha", "add"}, track.additive ? 1 : 0) == 1;
        track.scaleMode = choice(
            map, "scaleMode", {"hierarchy", "local", "shape"}, track.scaleMode);
        track.shape =
            choice(map, "shape", {"point", "line", "rectangle", "disk", "ring"},
                   track.shape);
        track.loop = boolean(map, "loop", track.loop);
        track.prewarm = boolean(map, "prewarm", track.prewarm);
        track.capacity = integer(map, "capacity", track.capacity);
        track.count =
            integer(map, "count", std::min(track.count, track.capacity));
        track.delay = number(map, "delay", track.delay);
        track.duration = number(map, "duration", track.duration);
        track.rate = number(map, "rate", track.rate);
        track.distanceRate = number(map, "distanceRate", track.distanceRate);
        track.extent = vector2(map, "extent", track.extent);
        track.radius = number(map, "radius", track.radius);
        track.innerRadius = number(map, "innerRadius", track.innerRadius);
        track.direction = number(map, "direction", track.direction);
        track.spread = number(map, "spread", track.spread);
        track.lifetime = vector2(map, "lifetime", track.lifetime);
        track.speed = vector2(map, "speed", track.speed);
        track.sizeMin = vector2(map, "sizeMin", track.sizeMin);
        track.sizeMax = vector2(map, "sizeMax", track.sizeMax);
        track.rotation = vector2(map, "rotation", track.rotation);
        track.angularVelocity =
            vector2(map, "angularVelocity", track.angularVelocity);
        track.colourMin = colour(map, "colourMin", track.colourMin);
        track.colourMax = colour(map, "colourMax", track.colourMax);
        track.gravity = vector2(map, "gravity", track.gravity);
        track.radialAcceleration =
            number(map, "radialAcceleration", track.radialAcceleration);
        track.tangentialAcceleration =
            number(map, "tangentialAcceleration", track.tangentialAcceleration);
        track.damping = number(map, "damping", track.damping);
        const auto rect =
            vector<4>(map, "textureRect",
                      {static_cast<float>(track.textureRect.position.x),
                       static_cast<float>(track.textureRect.position.y),
                       static_cast<float>(track.textureRect.size.x),
                       static_cast<float>(track.textureRect.size.y)});
        for (float component : rect) {
            if (component < 0 ||
                static_cast<double>(component) >
                    std::numeric_limits<int>::max() ||
                component != std::floor(component)) {
                throw std::invalid_argument("Invalid particle textureRect");
            }
        }
        track.textureRect = {
            {static_cast<int>(rect[0]), static_cast<int>(rect[1])},
            {static_cast<int>(rect[2]), static_cast<int>(rect[3])}};
        track.columns = integer(map, "columns", track.columns);
        track.rows = integer(map, "rows", track.rows);
        track.frameCount = integer(map, "frameCount", track.frameCount);
        track.frameRate = number(map, "frameRate", track.frameRate);
        track.randomStartFrame =
            boolean(map, "randomStartFrame", track.randomStartFrame);
        track.frameLoop = boolean(map, "frameLoop", track.frameLoop);
        track.offset = vector2(map, "offset", track.offset);
        track.rotationOffset =
            number(map, "rotationOffset", track.rotationOffset);
        track.scale = vector2(map, "scale", track.scale);
        if (const RuntimeData* curves =
                ludork::runtime::value_reader::findValue(map, "curves")) {
            const auto& values = ludork::runtime::value_reader::requireMap(
                *curves, "particle.curves");
            for (const auto& channel : emitterCurveChannels) {
                if (const RuntimeData* value =
                        ludork::runtime::value_reader::findValue(
                            values, channel.first)) {
                    Curve::CurveData& definition = track.curves.*channel.second;
                    definition = curve(*value, definition.defaultValue);
                }
            }
        }
        if (const RuntimeData* bursts =
                ludork::runtime::value_reader::findValue(map, "bursts")) {
            for (const RuntimeData& value :
                 ludork::runtime::value_reader::requireArray(
                     *bursts, "particle.bursts")) {
                const auto& item = ludork::runtime::value_reader::requireMap(
                    value, "particle.burst");
                ludork::runtime::graphics::EmitterBurst burst;
                burst.time = number(item, "time", burst.time);
                burst.count = integer(item, "count", burst.count);
                burst.cycles = integer(item, "cycles", burst.cycles);
                burst.interval = number(item, "interval", burst.interval);
                track.bursts.push_back(burst);
            }
        }
        result.tracks.push_back(std::move(track));
    }
    return result;
}
}  // namespace ludork::engine::emitters
