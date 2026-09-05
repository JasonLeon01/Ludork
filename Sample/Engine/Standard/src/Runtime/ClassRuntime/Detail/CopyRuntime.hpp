#pragma once

#include <sol2/forward.hpp>

namespace ludork::standard::class_runtime {

using NativeDeepCopyRecurse = sol::object (*)(void* context,
                                              const sol::object& value);

enum class NativeDeepCopyMode {
    TwoPhase,
    Deferred,
};

struct NativeDeepCopyProtocol {
    using Create = sol::object (*)(sol::state_view lua,
                                   const sol::object& source);
    using Populate = void (*)(sol::state_view lua, const sol::object& source,
                              const sol::object& destination,
                              NativeDeepCopyRecurse recurse, void* context);
    using Build = sol::object (*)(sol::state_view lua,
                                  const sol::object& source,
                                  NativeDeepCopyRecurse recurse, void* context);

    NativeDeepCopyMode mode = NativeDeepCopyMode::TwoPhase;
    Create create = nullptr;
    Populate populate = nullptr;
    Build build = nullptr;
};

void registerNativeDeepCopyProtocol(sol::state_view lua,
                                    const sol::table& nativeType,
                                    const NativeDeepCopyProtocol& protocol);

}  // namespace ludork::standard::class_runtime
