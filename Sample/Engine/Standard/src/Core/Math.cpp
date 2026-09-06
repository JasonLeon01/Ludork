#include <Math.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace ludork::standard::math {

namespace {

void requireFinite(double value) {
    if (!isFinite(value)) {
        throw std::invalid_argument("Expected a finite numeric value");
    }
}

double checkedResult(double value) {
    if (!isFinite(value)) {
        throw std::overflow_error("Numeric calculation overflowed");
    }
    return value;
}

std::int64_t checkedInteger(double value) {
    const double minimum =
        static_cast<double>(std::numeric_limits<std::int64_t>::min());
    if (value < minimum || value >= -minimum) {
        throw std::out_of_range("Numeric value is outside the integer range");
    }
    return static_cast<std::int64_t>(value);
}

std::uint64_t magnitude(std::int64_t value) {
    const std::uint64_t converted = static_cast<std::uint64_t>(value);
    return value < 0 ? std::uint64_t{0} - converted : converted;
}

std::int64_t checkedMagnitude(std::uint64_t value) {
    if (value >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::out_of_range("Numeric value is outside the integer range");
    }
    return static_cast<std::int64_t>(value);
}

}  // namespace

bool isFinite(double value) noexcept {
    return std::isfinite(value);
}

double clamp(double value, double minimum, double maximum) {
    requireFinite(value);
    requireFinite(minimum);
    requireFinite(maximum);
    if (minimum > maximum) {
        throw std::invalid_argument("Clamp minimum must not exceed maximum");
    }
    return std::clamp(value, minimum, maximum);
}

double lerp(double from, double to, double alpha) {
    requireFinite(from);
    requireFinite(to);
    requireFinite(alpha);
    return checkedResult(std::lerp(from, to, alpha));
}

std::int64_t round(double value) {
    requireFinite(value);
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) {
        return checkedInteger(lower);
    }
    if (fraction > 0.5) {
        return checkedInteger(lower + 1.0);
    }
    return checkedInteger(std::fmod(lower, 2.0) == 0.0 ? lower : lower + 1.0);
}

std::int64_t trunc(double value) {
    requireFinite(value);
    return checkedInteger(std::trunc(value));
}

bool isNearZero(double value, double epsilon) {
    requireFinite(value);
    requireFinite(epsilon);
    if (epsilon < 0.0) {
        throw std::invalid_argument("Epsilon must be non-negative");
    }
    return std::abs(value) < epsilon;
}

std::int64_t gcd(std::int64_t left, std::int64_t right) {
    return checkedMagnitude(std::gcd(magnitude(left), magnitude(right)));
}

std::int64_t lcm(std::int64_t left, std::int64_t right) {
    if (left == 0 || right == 0) {
        return 0;
    }
    const std::uint64_t a = magnitude(left);
    const std::uint64_t b = magnitude(right);
    const std::uint64_t quotient = a / std::gcd(a, b);
    const std::uint64_t maximum =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (quotient > maximum / b) {
        throw std::out_of_range("Numeric value is outside the integer range");
    }
    return static_cast<std::int64_t>(quotient * b);
}

std::int64_t sign(double value) {
    requireFinite(value);
    return (value > 0.0) - (value < 0.0);
}

double inverseLerp(double a, double b, double value) {
    requireFinite(a);
    requireFinite(b);
    requireFinite(value);
    if (a == b) {
        throw std::invalid_argument("Inverse lerp endpoints must differ");
    }
    const double span = checkedResult(b - a);
    const double offset = checkedResult(value - a);
    return checkedResult(offset / span);
}

double remap(double value, double inMin, double inMax, double outMin,
             double outMax) {
    return lerp(outMin, outMax, inverseLerp(inMin, inMax, value));
}

double smoothstep(double edge0, double edge1, double value) {
    requireFinite(edge0);
    requireFinite(edge1);
    requireFinite(value);
    if (edge0 >= edge1) {
        throw std::invalid_argument("Smoothstep edges must be increasing");
    }
    if (value <= edge0) {
        return 0.0;
    }
    if (value >= edge1) {
        return 1.0;
    }
    const double t = inverseLerp(edge0, edge1, value);
    return t * t * (3.0 - 2.0 * t);
}

double moveTowards(double current, double target, double maxDelta) {
    requireFinite(current);
    requireFinite(target);
    requireFinite(maxDelta);
    if (maxDelta < 0.0) {
        throw std::invalid_argument("Maximum delta must be non-negative");
    }
    const double delta = checkedResult(target - current);
    if (std::abs(delta) <= maxDelta) {
        return target;
    }
    return checkedResult(current + std::copysign(maxDelta, delta));
}

}  // namespace ludork::standard::math
