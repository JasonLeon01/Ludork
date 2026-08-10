#include <Curve.hpp>

#include <algorithm>
#include <cmath>

namespace {

std::string normaliseVectorInfinityMode(const std::string& mode) {
    return mode == "linear" ? "linear" : "constant";
}

std::string normaliseVectorInterpolation(const std::string& mode) {
    if (mode == "constant" || mode == "cubic") {
        return mode;
    }
    return "linear";
}

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

}  // namespace

Curve::Curve(CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(normaliseInfinityMode(data.preInfinity)),
      postInfinity(normaliseInfinityMode(data.postInfinity)),
      keys(std::move(data.keys)) {
    for (CurveKey& key : keys) {
        key.interpolation = normaliseInterpolation(key.interpolation);
    }
    std::stable_sort(keys.begin(), keys.end(),
                     [](const CurveKey& left, const CurveKey& right) {
                         return left.time < right.time;
                     });
}

std::shared_ptr<Curve> Curve::fromData(const CurveData& data) {
    return std::make_shared<Curve>(data);
}

CurveData Curve::toData() const {
    return {"curve", name, defaultValue, preInfinity, postInfinity, keys};
}

bool Curve::isEmpty() const {
    return keys.empty();
}

float Curve::getDuration() const {
    return keys.size() < 2 ? 0.0f : keys.back().time - keys.front().time;
}

float Curve::evaluate(float time) const {
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
    const auto ending =
        std::lower_bound(keys.begin(), keys.end(), time,
                         [](const CurveKey& key, float sampleTime) {
                             return key.time < sampleTime;
                         });
    return evaluateSegment(*(ending - 1), *ending, time);
}

std::string Curve::normaliseInfinityMode(const std::string& mode) {
    return mode == "linear" ? "linear" : "constant";
}

std::string Curve::normaliseInterpolation(const std::string& mode) {
    if (mode == "constant" || mode == "cubic") {
        return mode;
    }
    return "linear";
}

float Curve::evaluateSegment(const CurveKey& start, const CurveKey& ending,
                             float time) {
    const float duration = ending.time - start.time;
    if (duration <= 0.0f) {
        return ending.value;
    }
    if (start.interpolation == "constant") {
        return start.value;
    }
    const float alpha = (time - start.time) / duration;
    if (start.interpolation != "cubic") {
        return start.value + (ending.value - start.value) * alpha;
    }
    const float alpha2 = alpha * alpha;
    const float alpha3 = alpha2 * alpha;
    const float leaveTangent = start.leaveTangent * duration;
    const float arriveTangent = ending.arriveTangent * duration;
    return (2.0f * alpha3 - 3.0f * alpha2 + 1.0f) * start.value +
           (alpha3 - 2.0f * alpha2 + alpha) * leaveTangent +
           (-2.0f * alpha3 + 3.0f * alpha2) * ending.value +
           (alpha3 - alpha2) * arriveTangent;
}

float Curve::extrapolate(float time, const CurveKey& start,
                         const CurveKey& ending, const std::string& mode,
                         bool beforeFirst) {
    const CurveKey& edge = beforeFirst ? start : ending;
    const float duration = ending.time - start.time;
    if (mode != "linear" || duration <= 0.0f) {
        return edge.value;
    }
    const float slope = (ending.value - start.value) / duration;
    return edge.value + (time - edge.time) * slope;
}

Vector2Curve::Vector2Curve(Vector2CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(normaliseVectorInfinityMode(data.preInfinity)),
      postInfinity(normaliseVectorInfinityMode(data.postInfinity)),
      keys(std::move(data.keys)) {
    normaliseVectorKeys(keys);
}

std::shared_ptr<Vector2Curve> Vector2Curve::fromData(
    const Vector2CurveData& data) {
    return std::make_shared<Vector2Curve>(data);
}

Vector2CurveData Vector2Curve::toData() const {
    return {"vector2Curve", name,         defaultValue,
            preInfinity,    postInfinity, keys};
}

bool Vector2Curve::isEmpty() const {
    return keys.empty();
}

float Vector2Curve::getDuration() const {
    return vectorDuration(keys);
}

std::array<float, 2> Vector2Curve::evaluate(float time) const {
    return evaluateVector<Vector2CurveKey, 2>(keys, defaultValue, preInfinity,
                                              postInfinity, time);
}

Vector3Curve::Vector3Curve(Vector3CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(normaliseVectorInfinityMode(data.preInfinity)),
      postInfinity(normaliseVectorInfinityMode(data.postInfinity)),
      keys(std::move(data.keys)) {
    normaliseVectorKeys(keys);
}

std::shared_ptr<Vector3Curve> Vector3Curve::fromData(
    const Vector3CurveData& data) {
    return std::make_shared<Vector3Curve>(data);
}

Vector3CurveData Vector3Curve::toData() const {
    return {"vector3Curve", name,         defaultValue,
            preInfinity,    postInfinity, keys};
}

bool Vector3Curve::isEmpty() const {
    return keys.empty();
}

float Vector3Curve::getDuration() const {
    return vectorDuration(keys);
}

std::array<float, 3> Vector3Curve::evaluate(float time) const {
    return evaluateVector<Vector3CurveKey, 3>(keys, defaultValue, preInfinity,
                                              postInfinity, time);
}

Vector4Curve::Vector4Curve(Vector4CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(normaliseVectorInfinityMode(data.preInfinity)),
      postInfinity(normaliseVectorInfinityMode(data.postInfinity)),
      keys(std::move(data.keys)) {
    normaliseVectorKeys(keys);
}

std::shared_ptr<Vector4Curve> Vector4Curve::fromData(
    const Vector4CurveData& data) {
    return std::make_shared<Vector4Curve>(data);
}

Vector4CurveData Vector4Curve::toData() const {
    return {"vector4Curve", name,         defaultValue,
            preInfinity,    postInfinity, keys};
}

bool Vector4Curve::isEmpty() const {
    return keys.empty();
}

float Vector4Curve::getDuration() const {
    return vectorDuration(keys);
}

std::array<float, 4> Vector4Curve::evaluate(float time) const {
    return evaluateVector<Vector4CurveKey, 4>(keys, defaultValue, preInfinity,
                                              postInfinity, time);
}
