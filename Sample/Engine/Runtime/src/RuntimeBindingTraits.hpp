#pragma once

#include <LudorkRuntimeBinding/ValueTraits.hpp>
#include <Runtime/RuntimeValue.hpp>

namespace ludork::runtime::binding {

template <>
struct DynamicValueTraits<RuntimeValue> {
    static constexpr bool enabled = true;
};

template <>
struct OpaqueIdentityTraits<RuntimeIdentity> {
    static constexpr bool enabled = true;
};

}  // namespace ludork::runtime::binding
