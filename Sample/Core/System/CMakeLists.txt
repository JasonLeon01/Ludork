cmake_minimum_required(VERSION 3.21)

project(LudorkSystem LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_SYSTEM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

ludork_generate_lua_bindings(
    CoreSystem
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_SYSTEM_BINDINGS)

add_library(CoreSystem ${LUDORK_MODULE_LIBRARY_TYPE}
    ${LUDORK_SYSTEM_SOURCES}
    ${LUDORK_SYSTEM_BINDINGS})
add_library(Ludork::System ALIAS CoreSystem)
target_include_directories(CoreSystem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(CoreSystem PRIVATE zlibstatic)
ludork_configure_core_library(CoreSystem "${LUDORK_SYSTEM_BINDINGS}")
