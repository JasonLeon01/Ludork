#pragma once

#include <Runtime/RuntimeValue.hpp>
#include <Runtime/TypeSchema.hpp>

#include <string>
#include <vector>

namespace ludork::runtime::typed_data_impl {

TypeSchema parseSchema(RuntimeValueView value);
RuntimeValue schemaValue(const TypeSchema& type);
std::string schemaName(const TypeSchema& type);
bool isStandardSchema(const TypeSchema& type);
bool isContainerSchema(const TypeSchema& type);

}  // namespace ludork::runtime::typed_data_impl
