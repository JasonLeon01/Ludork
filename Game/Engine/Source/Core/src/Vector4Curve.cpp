#include <Vector4Curve.hpp>
#include <Vector4CurveKey.hpp>
#include <Vector4CurveData.hpp>

#include "Curve/CurveMath.hpp"

Vector4Curve::Vector4Curve(Vector4CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(ludork::engine::curve_detail::normaliseVectorInfinityMode(
          data.preInfinity)),
      postInfinity(ludork::engine::curve_detail::normaliseVectorInfinityMode(
          data.postInfinity)),
      keys(std::move(data.keys)) {
    ludork::engine::curve_detail::normaliseVectorKeys(keys);
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
    return ludork::engine::curve_detail::vectorDuration(keys);
}

std::array<float, 4> Vector4Curve::evaluate(float time) const {
    return ludork::engine::curve_detail::evaluateVector<Vector4CurveKey, 4>(
        keys, defaultValue, preInfinity, postInfinity, time);
}
