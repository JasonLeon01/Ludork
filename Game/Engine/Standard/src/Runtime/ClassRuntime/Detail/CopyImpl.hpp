#pragma once

#include <sol2/sol.hpp>

#include <unordered_map>

namespace ludork::standard::class_runtime::detail {

struct NativeDeepCopyContext {
    sol::state_view lua;
    std::unordered_map<const void*, sol::object>* visited = nullptr;
};

}  // namespace ludork::standard::class_runtime::detail
