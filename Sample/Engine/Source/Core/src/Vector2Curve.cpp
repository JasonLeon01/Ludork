#include <Vector2Curve.hpp>
#include <Vector2CurveKey.hpp>

#include "Curve/CurveMath.hpp"

Vector2Curve::Vector2Curve(Vector2Curve::Vector2CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(ludork::engine::curve_detail::normaliseVectorInfinityMode(
          data.preInfinity)),
      postInfinity(ludork::engine::curve_detail::normaliseVectorInfinityMode(
          data.postInfinity)),
      keys(std::move(data.keys)) {
    ludork::engine::curve_detail::normaliseVectorKeys(keys);
}

std::shared_ptr<Vector2Curve> Vector2Curve::fromData(
    const Vector2Curve::Vector2CurveData& data) {
    return std::make_shared<Vector2Curve>(data);
}

Vector2Curve::Vector2CurveData Vector2Curve::toData() const {
    return {"vector2Curve", name,         defaultValue,
            preInfinity,    postInfinity, keys};
}

bool Vector2Curve::isEmpty() const {
    return keys.empty();
}

float Vector2Curve::getDuration() const {
    return ludork::engine::curve_detail::vectorDuration(keys);
}

std::array<float, 2> Vector2Curve::evaluate(float time) const {
    return ludork::engine::curve_detail::evaluateVector<Vector2CurveKey, 2>(
        keys, defaultValue, preInfinity, postInfinity, time);
}
