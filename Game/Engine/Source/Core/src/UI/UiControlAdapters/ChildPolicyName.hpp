#pragma once

#include <UI/UiControlAdapterDescriptors.hpp>
#include <optional>
#include <string_view>

namespace ludork::engine::ui_control_adapter_detail {

inline std::optional<std::string_view> childPolicyName(UiChildPolicy policy) {
    switch (policy) {
        case UiChildPolicy::None:
            return "none";
        case UiChildPolicy::Single:
            return "single";
        case UiChildPolicy::Multiple:
            return "multiple";
    }
    return std::nullopt;
}

}  // namespace ludork::engine::ui_control_adapter_detail
