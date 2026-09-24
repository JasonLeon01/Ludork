#pragma once

namespace ludork::standard::class_runtime::protocol {

inline constexpr const char* NATIVE_POINTER_OWNERS_REGISTRY_KEY =
    "Ludork.Class.nativePointerOwners";
inline constexpr const char* DYNAMIC_NATIVE_WRITERS_REGISTRY_KEY =
    "Ludork.Class.dynamicNativeWriters";
inline constexpr const char* NATIVE_OBJECTS_FIELD = "__nativeObjects";
inline constexpr const char* COMPOSITE_MARKER_FIELD = "__LuaSFNativeComposite";

inline constexpr const char* CLASS_MARKER_FIELD = "__ludorkClass";
inline constexpr const char* CLASS_NAME_FIELD = "__name";
inline constexpr const char* CLASS_BASE_FIELD = "__base";
inline constexpr const char* CLASS_TYPE_FIELD = "__type";
inline constexpr const char* CLASS_METADATA_MODULE_FIELD = "__metadataModule";
inline constexpr const char* RUNTIME_METADATA_FIELD = "__runtimeMetadata";
inline constexpr const char* CLASS_GETTERS_FIELD = "__getters";
inline constexpr const char* CLASS_SETTERS_FIELD = "__setters";
inline constexpr const char* NATIVE_COPY_FIELD = "__copy";
inline constexpr const char* NATIVE_PROPERTIES_FIELD = "__nativeProperties";

}  // namespace ludork::standard::class_runtime::protocol
