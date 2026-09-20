#pragma once

#include <LuaGlue/LuaGlue.hpp>

namespace ludork::standard::class_runtime {

using NativeDeepCopyRecurse =
    lua_glue::Object (*)(void* context, const lua_glue::Object& value);

struct NativeDeepCopyProtocol {
    enum class NativeDeepCopyMode {
        TwoPhase,
        Deferred,
    };

    using Create = lua_glue::Object (*)(lua_glue::StateView lua,
                                        const lua_glue::Object& source);
    using Populate = void (*)(lua_glue::StateView lua,
                              const lua_glue::Object& source,
                              const lua_glue::Object& destination,
                              NativeDeepCopyRecurse recurse, void* context);
    using Build = lua_glue::Object (*)(lua_glue::StateView lua,
                                       const lua_glue::Object& source,
                                       NativeDeepCopyRecurse recurse,
                                       void* context);

    NativeDeepCopyMode mode = NativeDeepCopyMode::TwoPhase;
    Create create = nullptr;
    Populate populate = nullptr;
    Build build = nullptr;
};

void registerNativeDeepCopyProtocol(lua_glue::StateView lua,
                                    const lua_glue::Table& nativeType,
                                    const NativeDeepCopyProtocol& protocol);

}  // namespace ludork::standard::class_runtime
