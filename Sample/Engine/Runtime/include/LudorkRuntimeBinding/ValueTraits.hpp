#pragma once

namespace ludork::runtime::binding {

template <typename T>
struct DynamicValueTraits {
    static constexpr bool enabled = false;
};

template <typename T>
struct OpaqueIdentityTraits {
    static constexpr bool enabled = false;
};

}  // namespace ludork::runtime::binding
