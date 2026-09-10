#include <Vector3Curve.hpp>
#include <Vector3CurveKey.hpp>

#include "Curve/CurveMath.hpp"

Vector3Curve::Vector3Curve(Vector3Curve::Vector3CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(ludork::engine::curve_detail::normaliseVectorInfinityMode(
          data.preInfinity)),
      postInfinity(ludork::engine::curve_detail::normaliseVectorInfinityMode(
          data.postInfinity)),
      keys(std::move(data.keys)) {
    ludork::engine::curve_detail::normaliseVectorKeys(keys);
}

std::shared_ptr<Vector3Curve> Vector3Curve::fromData(
    const Vector3Curve::Vector3CurveData& data) {
    return std::make_shared<Vector3Curve>(data);
}

Vector3Curve::Vector3CurveData Vector3Curve::toData() const {
    return {"vector3Curve", name,         defaultValue,
            preInfinity,    postInfinity, keys};
}

bool Vector3Curve::isEmpty() const {
    return keys.empty();
}

float Vector3Curve::getDuration() const {
    return ludork::engine::curve_detail::vectorDuration(keys);
}

std::array<float, 3> Vector3Curve::evaluate(float time) const {
    return ludork::engine::curve_detail::evaluateVector<Vector3CurveKey, 3>(
        keys, defaultValue, preInfinity, postInfinity, time);
}
