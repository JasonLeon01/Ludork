set(LUDORK_CLASS_RUNTIME_NATIVE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Ownership.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Super.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Type.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Lookup.cpp")

target_sources(LudorkStandardClassRuntime PRIVATE
    ${LUDORK_CLASS_RUNTIME_NATIVE_SOURCES})

ludork_apply_msvc_debug_source_options(
    ${LUDORK_CLASS_RUNTIME_NATIVE_SOURCES})
