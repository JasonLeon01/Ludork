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
           std::initializer_list<const char*> choices) {
    const std::string value = string(map, key, *choices.begin());
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
Curve::CurveData curve(const RuntimeData& input, float fallback) {
    RuntimeData data = input;
    if (const std::string* key = data.getIf<std::string>()) {
        data = getJSONData("Data/Curves/" + *key + ".json");
    }
    const RuntimeData::Map& map =
        ludork::runtime::value_reader::requireMap(data, "particle.curve");
    Curve::CurveData result;
    result.defaultValue = number(map, "defaultValue", fallback);
    result.preInfinity = string(map, "preInfinity", "constant");
    result.postInfinity = string(map, "postInfinity", "constant");
    if (const RuntimeData* keys =
            ludork::runtime::value_reader::findValue(map, "keys")) {
        for (const RuntimeData& entry :
             ludork::runtime::value_reader::requireArray(
                 *keys, "particle.curve.keys")) {
            const RuntimeData::Map& key =
                ludork::runtime::value_reader::requireMap(entry,
                                                          "particle.curve.key");
            CurveKey item;
            item.time = number(key, "time", 0);
            item.value = number(key, "value", fallback);
            item.interpolation = string(key, "interpolation", "linear");
            item.arriveTangent = number(key, "arriveTangent", 0);
            item.leaveTangent = number(key, "leaveTangent", 0);
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
    result.name = string(root, "name", "");
    result.simulationRate = integer(root, "simulationRate", 60);
    result.seed = integer(root, "seed", 1);
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
        track.name = string(map, "name", "Track");
        track.texture = string(map, "texture", "");
        track.enabled = boolean(map, "enabled", true);
        track.resident = choice(map, "mode", {"emission", "resident"}) == 1;
        track.world = choice(map, "space", {"local", "world"}) == 1;
        track.additive = choice(map, "blend", {"alpha", "add"}) == 1;
        track.scaleMode =
            choice(map, "scaleMode", {"hierarchy", "local", "shape"});
        track.shape = choice(map, "shape",
                             {"point", "line", "rectangle", "disk", "ring"});
        track.loop = boolean(map, "loop", true);
        track.prewarm = boolean(map, "prewarm", false);
        track.capacity = integer(map, "capacity", 1024);
        track.count = integer(map, "count", std::min(32, track.capacity));
        track.delay = number(map, "delay", 0);
        track.duration = number(map, "duration", 2);
        track.rate = number(map, "rate", 30);
        track.distanceRate = number(map, "distanceRate", 0);
        track.extent = vector2(map, "extent", {32, 32});
        track.radius = number(map, "radius", 16);
        track.innerRadius = number(map, "innerRadius", 8);
        track.direction = number(map, "direction", -90);
        track.spread = number(map, "spread", 30);
        track.lifetime = vector2(map, "lifetime", {1, 2});
        track.speed = vector2(map, "speed", {20, 40});
        track.sizeMin = vector2(map, "sizeMin", {8, 8});
        track.sizeMax = vector2(map, "sizeMax", {16, 16});
        track.rotation = vector2(map, "rotation", {0, 360});
        track.angularVelocity = vector2(map, "angularVelocity", {0, 0});
        track.colourMin = vector<4>(map, "colourMin", {255, 255, 255, 255});
        track.colourMax = vector<4>(map, "colourMax", {255, 255, 255, 255});
        for (int index = 0; index < 4; ++index) {
            track.colourMin[index] /= 255;
            track.colourMax[index] /= 255;
        }
        track.gravity = vector2(map, "gravity", {});
        track.radialAcceleration = number(map, "radialAcceleration", 0);
        track.tangentialAcceleration = number(map, "tangentialAcceleration", 0);
        track.damping = number(map, "damping", 0);
        const auto rect = vector<4>(map, "textureRect", {0, 0, 0, 0});
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
        track.columns = integer(map, "columns", 1);
        track.rows = integer(map, "rows", 1);
        track.frameCount = integer(map, "frameCount", 1);
        track.frameRate = number(map, "frameRate", 0);
        track.randomStartFrame = boolean(map, "randomStartFrame", false);
        track.frameLoop = boolean(map, "frameLoop", true);
        track.offset = vector2(map, "offset", {});
        track.rotationOffset = number(map, "rotationOffset", 0);
        track.scale = vector2(map, "scale", {1, 1});
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
                ludork::runtime::graphics::EmitterBurst burst{
                    number(item, "time", 0), integer(item, "count", 0),
                    integer(item, "cycles", 1), number(item, "interval", 0)};
                track.bursts.push_back(burst);
            }
        }
        result.tracks.push_back(std::move(track));
    }
    return result;
}
}  // namespace ludork::engine::emitters
