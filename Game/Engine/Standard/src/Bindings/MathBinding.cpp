#include "Bindings.hpp"

#include <Math.hpp>
#include <Standard.hpp>
#include <LuaGlue/LuaGlue.hpp>

#include <stdexcept>

namespace ludork::standard::binding {

namespace {

bool isFinite(const lua_glue::Object& value) {
    return value.get_type() == lua_glue::Type::Number &&
           math::isFinite(value.as<lua_Number>());
}

double numberArgument(const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Number) {
        throw std::invalid_argument("Expected a numeric value");
    }
    return value.as<lua_Number>();
}

bool isInteger(const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Number) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    const bool integer = lua_isinteger(state, -1) != 0;
    lua_pop(state, 1);
    return integer;
}

std::int64_t integerArgument(const lua_glue::Object& value) {
    if (!isInteger(value)) {
        throw std::invalid_argument("Expected an integer value");
    }
    return value.as<lua_Integer>();
}

}  // namespace

void registerMath(lua_glue::StateView lua) {
    const lua_glue::Object rawMath =
        lua.globals().raw_get<lua_glue::Object>("math");
    if (!rawMath.is<lua_glue::Table>()) {
        throw std::runtime_error("Lua math library is not defined");
    }
    lua_glue::Table mathLibrary = rawMath.as<lua_glue::Table>();
    mathLibrary.set_function("isFinite", &isFinite);
    mathLibrary.set_function("clamp", [](const lua_glue::Object& value,
                                         const lua_glue::Object& minimum,
                                         const lua_glue::Object& maximum) {
        return math::clamp(numberArgument(value), numberArgument(minimum),
                           numberArgument(maximum));
    });
    mathLibrary.set_function(
        "lerp", [](const lua_glue::Object& from, const lua_glue::Object& to,
                   const lua_glue::Object& alpha) {
            return math::lerp(numberArgument(from), numberArgument(to),
                              numberArgument(alpha));
        });
    mathLibrary.set_function("round", [](const lua_glue::Object& value) {
        return isInteger(value) ? value.as<lua_Integer>()
                                : math::round(numberArgument(value));
    });
    mathLibrary.set_function("trunc", [](const lua_glue::Object& value) {
        return isInteger(value) ? value.as<lua_Integer>()
                                : math::trunc(numberArgument(value));
    });
    mathLibrary.set_function(
        "isNearZero", [](const lua_glue::Object& value,
                         const std::optional<lua_glue::Object>& epsilon) {
            return math::isNearZero(
                numberArgument(value),
                epsilon.has_value() ? numberArgument(*epsilon) : 0.1);
        });
    mathLibrary.set_function(
        "gcd", [](const lua_glue::Object& left, const lua_glue::Object& right) {
            return math::gcd(integerArgument(left), integerArgument(right));
        });
    mathLibrary.set_function(
        "lcm", [](const lua_glue::Object& left, const lua_glue::Object& right) {
            return math::lcm(integerArgument(left), integerArgument(right));
        });
    mathLibrary.set_function("sign", [](const lua_glue::Object& value) {
        return math::sign(numberArgument(value));
    });
    mathLibrary.set_function(
        "inverseLerp", [](const lua_glue::Object& a, const lua_glue::Object& b,
                          const lua_glue::Object& value) {
            return math::inverseLerp(numberArgument(a), numberArgument(b),
                                     numberArgument(value));
        });
    mathLibrary.set_function(
        "remap",
        [](const lua_glue::Object& value, const lua_glue::Object& inMin,
           const lua_glue::Object& inMax, const lua_glue::Object& outMin,
           const lua_glue::Object& outMax) {
            return math::remap(numberArgument(value), numberArgument(inMin),
                               numberArgument(inMax), numberArgument(outMin),
                               numberArgument(outMax));
        });
    mathLibrary.set_function("smoothstep", [](const lua_glue::Object& edge0,
                                              const lua_glue::Object& edge1,
                                              const lua_glue::Object& value) {
        return math::smoothstep(numberArgument(edge0), numberArgument(edge1),
                                numberArgument(value));
    });
    mathLibrary.set_function(
        "moveTowards",
        [](const lua_glue::Object& current, const lua_glue::Object& target,
           const lua_glue::Object& maxDelta) {
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
    binding::registerMath(lua_glue::StateView(state));
}

}  // namespace ludork::standard
