cmake_minimum_required(VERSION 3.21)

project(LudorkGlobal LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_GLOBAL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    list(APPEND LUDORK_GLOBAL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/NativeDisplayMac.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/NativeInputMethodMac.mm")
endif()

ludork_generate_lua_bindings(
    GlobalCore
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_GLOBAL_BINDINGS
    TYPE_REGISTRIES
        "Engine=${LUDORK_CORE_SOURCE_DIR}/Engine/include")

add_library(GlobalCore ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_GLOBAL_SOURCES}
    "${LUDORK_GLOBAL_BINDINGS}")
add_library(Ludork::Global ALIAS GlobalCore)
add_dependencies(GlobalCore GlobalCoreStub)
target_compile_definitions(GlobalCore PRIVATE
    LUDORK_GLOBAL_EXPORTS=1
    LUDORK_PLATFORM="${LUDORK_PLATFORM}")
target_include_directories(GlobalCore
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(GlobalCore
    PUBLIC
        Ludork::Engine
        Ludork::Standard
        SFML::Graphics
        SFML::Audio
        SFML::Network)
if(WIN32)
    target_link_libraries(GlobalCore PRIVATE Imm32 User32)
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    target_include_directories(GlobalCore PRIVATE
        "${LUDORK_CORE_SOURCE_DIR}/../LuaSF/third_party/SFML/src")
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
ludork_configure_core_library(GlobalCore "${LUDORK_GLOBAL_BINDINGS}")
