set(LUDORK_CLASS_RUNTIME_COMPOSITE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Index.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/NativeProperties.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Monitor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/NewIndex.cpp")

target_sources(LudorkStandardClassRuntime PRIVATE
    ${LUDORK_CLASS_RUNTIME_COMPOSITE_SOURCES})

ludork_apply_msvc_debug_source_options(
    ${LUDORK_CLASS_RUNTIME_COMPOSITE_SOURCES})
