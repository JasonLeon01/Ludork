#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <sol2/forward.hpp>

namespace ludork::runtime::detail {

RuntimeValue readRuntimeReference(const sol::object& value);

}  // namespace ludork::runtime::detail
