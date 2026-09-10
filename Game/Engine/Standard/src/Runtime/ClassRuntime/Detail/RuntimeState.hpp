#pragma once

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

inline constexpr const char* METHOD_OWNERS_KEY = "Ludork.Class.methodOwners";
inline constexpr const char* NATIVE_TYPE_CACHE_KEY =
    "Ludork.Class.nativeTypeCache";
inline constexpr const char* INSTANCES_KEY = "Ludork.Class.instances";
inline constexpr const char* COMPOSITE_METATABLE_KEY =
    "Ludork.Class.compositeMetatable";
inline constexpr const char* CONSTRUCTING_COMPOSITE_METATABLE_KEY =
    "Ludork.Class.constructingCompositeMetatable";
inline constexpr const char* NATIVE_OWNERS_KEY = "Ludork.Class.nativeOwners";
inline constexpr const char* SUPER_PROXY_CACHE_KEY =
    "Ludork.Class.superProxyCache";
inline constexpr const char* SUPER_PROXY_METATABLE_KEY =
    "Ludork.Class.superProxyMetatable";
inline constexpr const char* MONITOR_STATES_KEY = "Ludork.Class.monitorStates";
inline constexpr const char* LIFECYCLE_STATES_KEY =
    "Ludork.Class.lifecycleStates";
inline constexpr const char* DISPOSED_METATABLE_KEY =
    "Ludork.Class.disposedMetatable";
inline constexpr const char* SHUTTING_DOWN_KEY = "Ludork.Class.shuttingDown";

inline constexpr const char* NATIVE_METHOD_CACHE_FIELD = "__nativeMethodCache";
inline constexpr const char* FAST_INDEX_CACHE_FIELD = "__fastIndexCache";
inline constexpr const char* NATIVE_INITIALIZER_FIELD = "__classInit";
inline constexpr const char* NATIVE_INITIALIZING_FIELD = "__initializing";
inline constexpr const char* NATIVE_CONSTRUCTION_FAILED_FIELD =
    "__constructionFailed";
inline constexpr const char* NATIVE_CONSTRUCTING_ROOTS_FIELD =
    "__nativeConstructingRoots";
inline constexpr const char* NATIVE_DIRTY_PROPERTIES_FIELD =
    "__nativeDirtyProperties";
inline constexpr const char* NATIVE_CLASS_INDEX_FIELD =
    "__ludorkNativeClassIndex";
inline constexpr const char* NATIVE_CLASS_NEW_INDEX_FIELD =
    "__ludorkNativeClassNewIndex";
inline constexpr const char* NATIVE_CLASS_GUARD_FIELD =
    "__ludorkNativeClassGuard";
inline constexpr const char* NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD =
    "__classResolvedDefaults";
inline constexpr const char* NATIVE_COPY_FIELD = "__copy";

extern unsigned char nativeClassDefaultResolverKeyStorage;
extern unsigned char nativeDeepCopyProtocolsKeyStorage;

enum class LifecycleState : lua_Integer {
    Active = 0,
    Disposing = 1,
    Disposed = 2,
};

enum class FastIndexKind : lua_Integer {
    Value = 1,
    ScriptMember = 2,
    Getter = 3,
    NativeMember = 4,
};

enum class MroKind {
    Runtime,
    Native,
};

using NativeShadowSnapshot = std::vector<std::pair<sol::object, sol::object>>;

}  // namespace ludork::standard::class_runtime::detail
