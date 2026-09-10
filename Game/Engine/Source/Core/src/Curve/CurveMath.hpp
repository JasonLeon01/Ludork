#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace ludork::engine::curve_detail {

std::string normaliseVectorInfinityMode(const std::string& mode);
std::string normaliseVectorInterpolation(const std::string& mode);

template <typename Key>
void normaliseVectorKeys(std::vector<Key>& keys) {
    for (Key& key : keys) {
        key.interpolation = normaliseVectorInterpolation(key.interpolation);
    }
    std::stable_sort(keys.begin(), keys.end(),
                     [](const Key& left, const Key& right) {
                         return left.time < right.time;
                     });
}

template <typename Key, std::size_t Size>
std::array<float, Size> evaluateVectorSegment(const Key& start,
                                              const Key& ending, float time) {
    const float duration = ending.time - start.time;
    if (duration <= 0.0f) {
        return ending.value;
    }
    if (start.interpolation == "constant") {
        return start.value;
    }
    const float alpha = (time - start.time) / duration;
    std::array<float, Size> result{};
    if (start.interpolation != "cubic") {
        for (std::size_t component = 0; component < Size; ++component) {
            result[component] =
                start.value[component] +
                (ending.value[component] - start.value[component]) * alpha;
        }
        return result;
    }
    const float alpha2 = alpha * alpha;
    const float alpha3 = alpha2 * alpha;
    for (std::size_t component = 0; component < Size; ++component) {
        const float leaveTangent = start.leaveTangent[component] * duration;
        const float arriveTangent = ending.arriveTangent[component] * duration;
        result[component] =
            (2.0f * alpha3 - 3.0f * alpha2 + 1.0f) * start.value[component] +
            (alpha3 - 2.0f * alpha2 + alpha) * leaveTangent +
            (-2.0f * alpha3 + 3.0f * alpha2) * ending.value[component] +
            (alpha3 - alpha2) * arriveTangent;
    }
    return result;
}

template <typename Key, std::size_t Size>
std::array<float, Size> extrapolateVector(float time, const Key& start,
                                          const Key& ending,
                                          const std::string& mode,
                                          bool beforeFirst) {
    const Key& edge = beforeFirst ? start : ending;
    const float duration = ending.time - start.time;
    if (mode != "linear" || duration <= 0.0f) {
        return edge.value;
    }
    std::array<float, Size> result{};
    for (std::size_t component = 0; component < Size; ++component) {
        const float slope =
            (ending.value[component] - start.value[component]) / duration;
        result[component] = edge.value[component] + (time - edge.time) * slope;
    }
    return result;
}

template <typename Key, std::size_t Size>
std::array<float, Size> evaluateVector(
    const std::vector<Key>& keys, const std::array<float, Size>& defaultValue,
    const std::string& preInfinity, const std::string& postInfinity,
    float time) {
    if (keys.empty()) {
        return defaultValue;
    }
    if (keys.size() == 1) {
        return keys.front().value;
    }
    if (time <= keys.front().time) {
        return extrapolateVector<Key, Size>(time, keys[0], keys[1], preInfinity,
                                            true);
    }
    if (time >= keys.back().time) {
        return extrapolateVector<Key, Size>(time, keys[keys.size() - 2],
                                            keys.back(), postInfinity, false);
    }
    const auto ending = std::lower_bound(keys.begin(), keys.end(), time,
                                         [](const Key& key, float sampleTime) {
                                             return key.time < sampleTime;
                                         });
    return evaluateVectorSegment<Key, Size>(*(ending - 1), *ending, time);
}

template <typename Key>
float vectorDuration(const std::vector<Key>& keys) {
    return keys.size() < 2 ? 0.0f : keys.back().time - keys.front().time;
}

}  // namespace ludork::engine::curve_detail
