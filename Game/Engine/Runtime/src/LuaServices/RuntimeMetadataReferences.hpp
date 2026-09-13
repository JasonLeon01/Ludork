#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <string>
#include <vector>

namespace ludork::runtime::detail {

struct ComponentTypeReference {
    std::string name;
    RuntimeValue type;
    std::string module;
};

RuntimeValue::Map classConfigReferences(const RuntimeValue& owner);
std::vector<ComponentTypeReference> componentTypeReferences(
    const RuntimeValue& owner);

}  // namespace ludork::runtime::detail
