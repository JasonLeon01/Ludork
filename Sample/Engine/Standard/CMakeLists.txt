cmake_minimum_required(VERSION 3.21)

project(LudorkStandard LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_STANDARD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER LUDORK_STANDARD_SOURCES EXCLUDE REGEX
    ".*/src/Runtime/ClassRuntime/.*\\.cpp$")

ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeConversion.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeDictionary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeHash.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeIteration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeSequence.cpp")

add_library(LudorkStandard ${LUDORK_RUNTIME_LIBRARY_TYPE} ${LUDORK_STANDARD_SOURCES})
add_library(Ludork::Standard ALIAS LudorkStandard)
target_compile_features(LudorkStandard PUBLIC cxx_std_20)
target_compile_definitions(LudorkStandard
    PRIVATE
        LUDORK_STANDARD_EXPORTS=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}"
        LUDORK_SAVE_AS_LDC=$<BOOL:${LUDORK_SAVE_AS_LDC}>
    PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${LUDORK_DEBUG_SOL_SAFETIES}>>:SOL_ALL_SAFETIES_ON=1>
        SOL_SAFE_NUMERICS=1
        SOL_USE_INTEROP=1
        SOL_DEFAULT_AUTOMAGICAL_USERTYPES=0
        SOL_USERTYPE_TYPE_BINDING_INFO=0)
target_include_directories(LudorkStandard
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${LUDORK_LUASF_CONSUMER_INCLUDES}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(LudorkStandard
    PRIVATE
        LuaSF::LuaSF
        LuaSF::Lua
        SFML::Network
        Threads::Threads
        zlibstatic)
if(WIN32)
    target_link_libraries(LudorkStandard PRIVATE Psapi)
endif()
set_target_properties(LudorkStandard PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF)
ludork_set_runtime_output(LudorkStandard)

add_subdirectory(src/Runtime/ClassRuntime)
target_link_libraries(LudorkStandard PRIVATE LudorkStandardClassRuntime)
