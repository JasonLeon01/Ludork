#pragma once

#include <sol2/sol.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace ludork::standard::class_runtime::detail {

struct DisposeSnapshot {
    struct NativeDisposeTarget {
        sol::table root;
        sol::object nativeObject;
        bool requiresHook{};
    };

    sol::table fields;
    sol::table classTable;
    std::optional<std::size_t> instanceId;
    std::vector<NativeDisposeTarget> nativeTargets;
};

}  // namespace ludork::standard::class_runtime::detail
