cmake_minimum_required(VERSION 3.21)

project(LudorkGlobalFunctions LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_GLOBAL_FUNCTIONS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

ludork_generate_lua_bindings(
    GlobalFunctions
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_GLOBAL_FUNCTIONS_BINDINGS
    INCLUDE_DIRECTORIES "${LUDORK_RUNTIME_SOURCE_DIR}/include"
    TYPE_REGISTRIES
        "Engine=${LUDORK_CORE_SOURCE_DIR}/Core/include"
        "Engine=${LUDORK_RUNTIME_SOURCE_DIR}/include/Runtime"
        "GlobalCore=${LUDORK_CORE_SOURCE_DIR}/Global/include")

add_library(GlobalFunctions ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_GLOBAL_FUNCTIONS_SOURCES}
    ${LUDORK_GLOBAL_FUNCTIONS_BINDINGS})
add_library(Ludork::GlobalFunctions ALIAS GlobalFunctions)
ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Components.cpp")
target_compile_definitions(GlobalFunctions PRIVATE
    LUDORK_PLATFORM="${LUDORK_PLATFORM}")
target_include_directories(GlobalFunctions
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(GlobalFunctions
    PUBLIC
        Ludork::Global
        SFML::Graphics
        SFML::Audio)
ludork_configure_core_library(
    GlobalFunctions
    "${LUDORK_GLOBAL_FUNCTIONS_BINDINGS}"
    "${LUDORK_GLOBAL_FUNCTIONS_BINDINGS_TRAITS}")
