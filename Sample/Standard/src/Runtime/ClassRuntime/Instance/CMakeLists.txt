set(LUDORK_CLASS_RUNTIME_INSTANCE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/NativeConstruction.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Lifecycle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Allocation.cpp")

target_sources(LudorkStandardClassRuntime PRIVATE
    ${LUDORK_CLASS_RUNTIME_INSTANCE_SOURCES})

ludork_apply_msvc_debug_source_options(
    ${LUDORK_CLASS_RUNTIME_INSTANCE_SOURCES})
