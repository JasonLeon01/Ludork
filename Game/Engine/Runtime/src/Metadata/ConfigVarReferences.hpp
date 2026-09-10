#pragma once

#include <Runtime/RuntimeValue.hpp>

namespace ludork::runtime::detail {

RuntimeValue::Map parseConfigVarReferences(const RuntimeValue& metadata);

}  // namespace ludork::runtime::detail
