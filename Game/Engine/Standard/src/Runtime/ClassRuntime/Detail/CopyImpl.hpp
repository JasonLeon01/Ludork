#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <unordered_map>

namespace ludork::standard::class_runtime::detail {

struct NativeDeepCopyContext {
    lua_glue::StateView lua;
    std::unordered_map<const void*, lua_glue::Object>* visited = nullptr;
};

}  // namespace ludork::standard::class_runtime::detail
