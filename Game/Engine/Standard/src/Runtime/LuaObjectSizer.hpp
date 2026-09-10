#pragma once

#include <sol2/forward.hpp>

#include <cstdint>

namespace ludork::standard {

std::int64_t luaObjectSize(const sol::object& value);

}
