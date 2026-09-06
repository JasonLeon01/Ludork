#pragma once

#include <StandardApi.hpp>

#include <cstdint>

namespace ludork::standard::math {

LUDORK_STANDARD_API bool isFinite(double value) noexcept;
LUDORK_STANDARD_API double clamp(double value, double minimum, double maximum);
LUDORK_STANDARD_API double lerp(double from, double to, double alpha);
LUDORK_STANDARD_API std::int64_t round(double value);
LUDORK_STANDARD_API std::int64_t trunc(double value);
LUDORK_STANDARD_API bool isNearZero(double value, double epsilon = 0.1);
LUDORK_STANDARD_API std::int64_t gcd(std::int64_t left, std::int64_t right);
LUDORK_STANDARD_API std::int64_t lcm(std::int64_t left, std::int64_t right);
LUDORK_STANDARD_API std::int64_t sign(double value);
LUDORK_STANDARD_API double inverseLerp(double a, double b, double value);
LUDORK_STANDARD_API double remap(double value, double inMin, double inMax,
                                 double outMin, double outMax);
LUDORK_STANDARD_API double smoothstep(double edge0, double edge1, double value);
LUDORK_STANDARD_API double moveTowards(double current, double target,
                                       double maxDelta);

}  // namespace ludork::standard::math
