#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace ludork::standard::class_runtime::detail {

struct DisposeSnapshot {
    struct NativeDisposeTarget {
        lua_glue::Table root;
        lua_glue::Object nativeObject;
        bool requiresHook{};
    };

    lua_glue::Table fields;
    lua_glue::Table classTable;
    std::optional<std::size_t> instanceId;
    std::vector<NativeDisposeTarget> nativeTargets;
};

}  // namespace ludork::standard::class_runtime::detail
