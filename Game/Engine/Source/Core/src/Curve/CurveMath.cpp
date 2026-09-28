#include "CurveMath.hpp"

namespace ludork::engine::curve_detail {

std::string normaliseInfinityMode(const std::string& mode) {
    return mode == "linear" ? "linear" : "constant";
}

std::string normaliseInterpolation(const std::string& mode) {
    if (mode == "constant" || mode == "cubic") {
        return mode;
    }
    return "linear";
}

}  // namespace ludork::engine::curve_detail
