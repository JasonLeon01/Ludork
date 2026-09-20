#pragma once
#include <Runtime/RuntimeValue.hpp>
namespace ludork::runtime::components {
struct ComponentFieldTarget {
    RuntimeValue component;
    RuntimeValue componentType;
    bool found = false;
};
}  // namespace ludork::runtime::components
