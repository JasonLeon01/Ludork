#include "CurveMath.hpp"

namespace ludork::engine::curve_detail {

std::string normaliseInfinityMode(const std::string& mode) {
    return mode == CurveInfinityMode::LINEAR ? CurveInfinityMode::LINEAR
                                             : CurveInfinityMode::CONSTANT;
}

std::string normaliseInterpolation(const std::string& mode) {
    if (mode == CurveInterpolation::CONSTANT ||
        mode == CurveInterpolation::CUBIC) {
        return mode;
    }
    return CurveInterpolation::LINEAR;
}

}  // namespace ludork::engine::curve_detail
