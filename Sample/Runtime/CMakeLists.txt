cmake_minimum_required(VERSION 3.21)

project(LudorkRuntime LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MetadataRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeProviders.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeReflection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeServiceCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeServiceInternals.cpp")

add_library(LudorkRuntime ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_RUNTIME_SOURCES})
add_library(Ludork::Runtime ALIAS LudorkRuntime)

target_compile_features(LudorkRuntime PUBLIC cxx_std_20)
target_compile_definitions(LudorkRuntime
    PRIVATE LUDORK_RUNTIME_EXPORTS=1
    PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${LUDORK_DEBUG_SOL_SAFETIES}>>:SOL_ALL_SAFETIES_ON=1>
        SOL_SAFE_NUMERICS=1
        SOL_USE_INTEROP=1
        SOL_DEFAULT_AUTOMAGICAL_USERTYPES=0
        SOL_USERTYPE_TYPE_BINDING_INFO=0)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    target_compile_definitions(LudorkRuntime PUBLIC
        _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()
target_include_directories(LudorkRuntime
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${LUDORK_LUASF_CONSUMER_INCLUDES}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(LudorkRuntime
    PUBLIC
        Ludork::Standard
        LuaSF::LuaSF
        LuaSF::Lua
        SFML::Graphics
        SFML::Audio
        SFML::Network
    PRIVATE
        zlibstatic)

set_target_properties(LudorkRuntime PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    INTERPROCEDURAL_OPTIMIZATION_DEBUG ${LUDORK_OPTIMIZE_DEBUG}
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF)
if(MINGW)
    set_target_properties(LudorkRuntime PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
        INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
endif()
ludork_set_runtime_output(LudorkRuntime)
