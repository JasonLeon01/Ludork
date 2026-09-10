#pragma once

namespace ludork::standard::class_runtime::protocol {

inline constexpr const char* NATIVE_POINTER_OWNERS_REGISTRY_KEY =
    "Ludork.Class.nativePointerOwners";
inline constexpr const char* DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY =
    "Ludork.Class.dynamicNativeWriters";
inline constexpr const char* NATIVE_OBJECTS_FIELD = "__nativeObjects";
inline constexpr const char* COMPOSITE_MARKER_FIELD = "__LuaSFNativeComposite";

}  // namespace ludork::standard::class_runtime::protocol
