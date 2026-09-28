#include <Curve.hpp>
#include <CurveKey.hpp>

#include "Curve/CurveMath.hpp"

Curve::Curve(Curve::CurveData data)
    : name(std::move(data.name)),
      defaultValue(data.defaultValue),
      preInfinity(ludork::engine::curve_detail::normaliseInfinityMode(
          data.preInfinity)),
      postInfinity(ludork::engine::curve_detail::normaliseInfinityMode(
          data.postInfinity)),
      keys(std::move(data.keys)) {
    ludork::engine::curve_detail::normaliseKeys(keys);
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
    return ludork::engine::curve_detail::duration(keys);
}

float Curve::evaluate(float time) const {
    return ludork::engine::curve_detail::evaluate(
        keys, defaultValue, preInfinity, postInfinity, time);
}
