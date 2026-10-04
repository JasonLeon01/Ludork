#pragma once

#include <Runtime/RuntimeValue.hpp>
#include <Runtime/TypeSchema.hpp>

#include <string>

namespace ludork::runtime::detail {

void validateEnumModuleName(const std::string& moduleName);
TypeSchema enumValueType(RuntimeValueView values,
                         const TypeSchema& enumeration);

}  // namespace ludork::runtime::detail
