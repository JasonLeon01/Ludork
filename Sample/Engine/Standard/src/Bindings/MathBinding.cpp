#include "Bindings.hpp"

#include <Math.hpp>
#include <Standard.hpp>
#include <sol2/sol.hpp>

#include <stdexcept>

namespace ludork::standard::binding {

namespace {

bool isFinite(const sol::object& value) {
    return value.get_type() == sol::type::number &&
           math::isFinite(value.as<lua_Number>());
}

double numberArgument(const sol::object& value) {
    if (value.get_type() != sol::type::number) {
        throw std::invalid_argument("Expected a numeric value");
    }
    return value.as<lua_Number>();
}

bool isInteger(const sol::object& value) {
    if (value.get_type() != sol::type::number) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    const bool integer = lua_isinteger(state, -1) != 0;
    lua_pop(state, 1);
    return integer;
}

std::int64_t integerArgument(const sol::object& value) {
    if (!isInteger(value)) {
        throw std::invalid_argument("Expected an integer value");
    }
    return value.as<lua_Integer>();
}

}  // namespace

void registerMath(sol::state_view lua) {
    const sol::object rawMath = lua.globals().raw_get<sol::object>("math");
    if (!rawMath.is<sol::table>()) {
        throw std::runtime_error("Lua math library is not defined");
    }
    sol::table mathLibrary = rawMath.as<sol::table>();
    mathLibrary.set_function("isFinite", &isFinite);
    mathLibrary.set_function(
        "clamp", [](const sol::object& value, const sol::object& minimum,
                    const sol::object& maximum) {
            return math::clamp(numberArgument(value), numberArgument(minimum),
                               numberArgument(maximum));
        });
    mathLibrary.set_function(
        "lerp", [](const sol::object& from, const sol::object& to,
                   const sol::object& alpha) {
            return math::lerp(numberArgument(from), numberArgument(to),
                              numberArgument(alpha));
        });
    mathLibrary.set_function("round", [](const sol::object& value) {
        return isInteger(value) ? value.as<lua_Integer>()
                                : math::round(numberArgument(value));
    });
    mathLibrary.set_function("trunc", [](const sol::object& value) {
        return isInteger(value) ? value.as<lua_Integer>()
                                : math::trunc(numberArgument(value));
    });
    mathLibrary.set_function(
        "isNearZero", [](const sol::object& value,
                         const sol::optional<sol::object>& epsilon) {
            return math::isNearZero(
                numberArgument(value),
                epsilon.has_value() ? numberArgument(*epsilon) : 0.1);
        });
    mathLibrary.set_function(
        "gcd", [](const sol::object& left, const sol::object& right) {
            return math::gcd(integerArgument(left), integerArgument(right));
        });
    mathLibrary.set_function(
        "lcm", [](const sol::object& left, const sol::object& right) {
            return math::lcm(integerArgument(left), integerArgument(right));
        });
    mathLibrary.set_function("sign", [](const sol::object& value) {
        return math::sign(numberArgument(value));
    });
    mathLibrary.set_function(
        "inverseLerp", [](const sol::object& a, const sol::object& b,
                          const sol::object& value) {
            return math::inverseLerp(numberArgument(a), numberArgument(b),
                                     numberArgument(value));
        });
    mathLibrary.set_function(
        "remap", [](const sol::object& value, const sol::object& inMin,
                    const sol::object& inMax, const sol::object& outMin,
                    const sol::object& outMax) {
            return math::remap(numberArgument(value), numberArgument(inMin),
                               numberArgument(inMax), numberArgument(outMin),
                               numberArgument(outMax));
        });
    mathLibrary.set_function("smoothstep", [](const sol::object& edge0,
                                              const sol::object& edge1,
                                              const sol::object& value) {
        return math::smoothstep(numberArgument(edge0), numberArgument(edge1),
                                numberArgument(value));
    });
    mathLibrary.set_function(
        "moveTowards", [](const sol::object& current, const sol::object& target,
                          const sol::object& maxDelta) {
            return math::moveTowards(numberArgument(current),
                                     numberArgument(target),
                                     numberArgument(maxDelta));
        });
}

}  // namespace ludork::standard::binding

namespace ludork::standard {

void initializeMath(lua_State* state) {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    binding::registerMath(sol::state_view(state));
}

}  // namespace ludork::standard
