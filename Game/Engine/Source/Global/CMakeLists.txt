cmake_minimum_required(VERSION 3.21)

project(LudorkGlobal LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_GLOBAL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(APPLE)
    enable_language(OBJCXX)
endif()
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND LUDORK_GLOBAL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/System/Platform/NativeDisplayMac.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/System/Platform/NativeInputMethodMac.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/System/Platform/DesktopTextInputHostMac.mm")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND LUDORK_GLOBAL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/System/Platform/TextInputHostIOS.mm")
endif()

ludork_generate_lua_bindings(
    GlobalCore
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_GLOBAL_BINDINGS
    INCLUDE_DIRECTORIES "${LUDORK_RUNTIME_SOURCE_DIR}/include"
    TYPE_REGISTRIES
        "Engine=${LUDORK_CORE_SOURCE_DIR}/Core/include"
        "Engine=${LUDORK_RUNTIME_SOURCE_DIR}/include/Runtime")

add_library(GlobalCore ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_GLOBAL_SOURCES}
    ${LUDORK_GLOBAL_BINDINGS})
add_library(Ludork::Global ALIAS GlobalCore)
target_link_libraries(GlobalCore PRIVATE Ludork::RuntimeConstants)
target_compile_definitions(GlobalCore PRIVATE
    LUDORK_GLOBAL_EXPORTS=1
    LUDORK_PLATFORM="${LUDORK_PLATFORM}")
target_include_directories(GlobalCore
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/System/Config")
target_link_libraries(GlobalCore
    PUBLIC
        Ludork::Engine
        Ludork::Standard
        SFML::Graphics
        SFML::Audio
        SFML::Network
    PRIVATE
        LuaSF::LuaSF
        LuaSF::Lua)
if(WIN32)
    target_link_libraries(GlobalCore PRIVATE Imm32 User32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
    target_link_libraries(GlobalCore PRIVATE "${UIKIT_FRAMEWORK}")
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    target_include_directories(GlobalCore PRIVATE
        "${LUDORK_SFML_SOURCE_DIR}/src")
    target_link_libraries(GlobalCore PRIVATE "${APPKIT_FRAMEWORK}")
endif()
if(LUDORK_ENABLE_FFMPEG)
    target_compile_definitions(GlobalCore PRIVATE LUDORK_HAS_FFMPEG=1)
    target_link_libraries(GlobalCore PRIVATE
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avutil
        FFmpeg::swscale
        FFmpeg::swresample)
else()
    target_compile_definitions(GlobalCore PRIVATE LUDORK_HAS_FFMPEG=0)
endif()
ludork_configure_core_library(
    GlobalCore
    "${LUDORK_GLOBAL_BINDINGS}"
    "${LUDORK_GLOBAL_BINDINGS_TRAITS}")
