#include <Curve.hpp>
#include <CurveKey.hpp>

#include <algorithm>
#include <cmath>

Curve::Curve(Curve::CurveData data)
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

std::shared_ptr<Curve> Curve::fromData(const Curve::CurveData& data) {
    return std::make_shared<Curve>(data);
}

Curve::CurveData Curve::toData() const {
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
