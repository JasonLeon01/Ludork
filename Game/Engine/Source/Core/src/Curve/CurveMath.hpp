#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace ludork::engine::curve_detail {

std::string normaliseInfinityMode(const std::string& mode);
std::string normaliseInterpolation(const std::string& mode);

template <typename Key>
void normaliseKeys(std::vector<Key>& keys) {
    for (Key& key : keys) {
        key.interpolation = normaliseInterpolation(key.interpolation);
    }
    std::stable_sort(keys.begin(), keys.end(),
                     [](const Key& left, const Key& right) {
                         return left.time < right.time;
                     });
}

inline float componentValue(float value, std::size_t) {
    return value;
}

template <std::size_t Size>
float componentValue(const std::array<float, Size>& value,
                     std::size_t component) {
    return value[component];
}

template <typename Operation>
float mapComponents(float value, Operation operation) {
    return operation(value, 0);
}

template <std::size_t Size, typename Operation>
std::array<float, Size> mapComponents(const std::array<float, Size>& value,
                                      Operation operation) {
    std::array<float, Size> result{};
    for (std::size_t component = 0; component < Size; ++component) {
        result[component] = operation(value[component], component);
    }
    return result;
}

template <typename Key>
auto evaluateSegment(const Key& start, const Key& ending, float time) {
    const float duration = ending.time - start.time;
    if (duration <= 0.0f) {
        return ending.value;
    }
    if (start.interpolation == "constant") {
        return start.value;
    }
    const float alpha = (time - start.time) / duration;
    if (start.interpolation != "cubic") {
        return mapComponents(start.value, [&](float value,
                                              std::size_t component) {
            return value +
                   (componentValue(ending.value, component) - value) * alpha;
        });
    }
    const float alpha2 = alpha * alpha;
    const float alpha3 = alpha2 * alpha;
    return mapComponents(start.value, [&](float value, std::size_t component) {
        const float leaveTangent =
            componentValue(start.leaveTangent, component) * duration;
        const float arriveTangent =
            componentValue(ending.arriveTangent, component) * duration;
        return (2.0f * alpha3 - 3.0f * alpha2 + 1.0f) * value +
               (alpha3 - 2.0f * alpha2 + alpha) * leaveTangent +
               (-2.0f * alpha3 + 3.0f * alpha2) *
                   componentValue(ending.value, component) +
               (alpha3 - alpha2) * arriveTangent;
    });
}

template <typename Key>
auto extrapolate(float time, const Key& start, const Key& ending,
                 const std::string& mode, bool beforeFirst) {
    const Key& edge = beforeFirst ? start : ending;
    const float duration = ending.time - start.time;
    if (mode != "linear" || duration <= 0.0f) {
        return edge.value;
    }
    return mapComponents(edge.value, [&](float value, std::size_t component) {
        const float slope = (componentValue(ending.value, component) -
                             componentValue(start.value, component)) /
                            duration;
        return value + (time - edge.time) * slope;
    });
}

template <typename Key, typename Value>
Value evaluate(const std::vector<Key>& keys, const Value& defaultValue,
               const std::string& preInfinity, const std::string& postInfinity,
               float time) {
    if (keys.empty()) {
        return defaultValue;
    }
    if (keys.size() == 1) {
        return keys.front().value;
    }
    if (time <= keys.front().time) {
        return extrapolate(time, keys[0], keys[1], preInfinity, true);
    }
    if (time >= keys.back().time) {
        return extrapolate(time, keys[keys.size() - 2], keys.back(),
                           postInfinity, false);
    }
    const auto ending = std::lower_bound(keys.begin(), keys.end(), time,
                                         [](const Key& key, float sampleTime) {
                                             return key.time < sampleTime;
                                         });
    return evaluateSegment(*(ending - 1), *ending, time);
}

template <typename Key>
float duration(const std::vector<Key>& keys) {
    return keys.size() < 2 ? 0.0f : keys.back().time - keys.front().time;
}

}  // namespace ludork::engine::curve_detail
