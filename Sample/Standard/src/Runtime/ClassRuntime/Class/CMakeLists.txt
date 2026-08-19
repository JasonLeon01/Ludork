set(LUDORK_CLASS_RUNTIME_CLASS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Construction.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Definition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Introspection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Module.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ClassServices.cpp")

target_sources(LudorkStandardClassRuntime PRIVATE
    ${LUDORK_CLASS_RUNTIME_CLASS_SOURCES})

ludork_apply_msvc_debug_source_options(
    ${LUDORK_CLASS_RUNTIME_CLASS_SOURCES})
