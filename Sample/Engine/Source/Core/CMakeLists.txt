cmake_minimum_required(VERSION 3.21)

project(LudorkEngine LANGUAGES CXX)

if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
endif()

if(LUDORK_BUILD_SAMPLE_APPLICATION)
file(GLOB_RECURSE LUDORK_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND LUDORK_ENGINE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Input/InputService/Platform/PlatformInputBridgeMac.mm")
endif()

ludork_generate_lua_bindings(
    Engine
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_ENGINE_BINDINGS
    HEADER_DIRECTORIES "${LUDORK_RUNTIME_SOURCE_DIR}/include/Runtime"
    INCLUDE_DIRECTORIES "${LUDORK_RUNTIME_SOURCE_DIR}/include")

add_library(Engine ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_ENGINE_SOURCES}
    ${LUDORK_ENGINE_BINDINGS})
add_library(Ludork::Engine ALIAS Engine)

target_compile_definitions(Engine PRIVATE
    LUDORK_ENGINE_EXPORTS=1
    LUDORK_PLATFORM="${LUDORK_PLATFORM}")
target_include_directories(Engine
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/UI")
target_link_libraries(Engine
    PUBLIC
        Ludork::Runtime
        SFML::Graphics
        SFML::Audio
        SFML::Network
    PRIVATE
        Ludork::Standard)
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    target_link_libraries(Engine PRIVATE deviceinfo_ndk.z)
endif()
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    target_link_libraries(Engine PRIVATE
        "${APPKIT_FRAMEWORK}"
        "${COREGRAPHICS_FRAMEWORK}")
endif()
ludork_configure_core_library(
    Engine
    "${LUDORK_ENGINE_BINDINGS}"
    "${LUDORK_ENGINE_BINDINGS_TRAITS}")
endif()

if(LUDORK_BUILD_UI_PREVIEW_HOST_RUNTIME)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkUiPreviewHostRuntime.cmake")
    ludork_add_ui_preview_host_runtime(UiPreviewHostRuntime)
endif()
