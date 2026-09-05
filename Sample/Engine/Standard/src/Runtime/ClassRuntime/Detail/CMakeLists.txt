set(LUDORK_CLASS_RUNTIME_DETAIL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ClassNativeInterop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LuaSupport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Hierarchy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Copy.cpp")

target_sources(LudorkStandardClassRuntime PRIVATE
    ${LUDORK_CLASS_RUNTIME_DETAIL_SOURCES})

ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LuaSupport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Hierarchy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Copy.cpp")
