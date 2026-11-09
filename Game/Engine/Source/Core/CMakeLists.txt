cmake_minimum_required(VERSION 3.21)

project(LudorkEngine LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkEnginePlatform.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkCoreShared.cmake")

if(LUDORK_BUILD_GAME_APPLICATION)
file(GLOB_RECURSE LUDORK_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER LUDORK_ENGINE_SOURCES EXCLUDE REGEX "/src/Shared/")

ludork_generate_lua_bindings(
    Engine
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_ENGINE_BINDINGS
    HEADER_DIRECTORIES "${LUDORK_RUNTIME_SOURCE_DIR}/include/Runtime"
    INCLUDE_DIRECTORIES "${LUDORK_RUNTIME_SOURCE_DIR}/include")

add_library(Engine ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_ENGINE_SOURCES})
file(GLOB_RECURSE LUDORK_MATERIAL_CACHE_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Materials/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Materials/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Graphics/Materials/*.hpp")
list(SORT LUDORK_MATERIAL_CACHE_INPUTS)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${LUDORK_MATERIAL_CACHE_INPUTS})
set(LUDORK_MATERIAL_CACHE_IDENTITY "")
foreach(input IN LISTS LUDORK_MATERIAL_CACHE_INPUTS)
    file(SHA256 "${input}" input_hash)
    string(APPEND LUDORK_MATERIAL_CACHE_IDENTITY "${input_hash}\n")
endforeach()
string(SHA256 LUDORK_MATERIAL_CACHE_FINGERPRINT "${LUDORK_MATERIAL_CACHE_IDENTITY}")
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/MaterialCacheCompilerFingerprint.hpp"
    CONTENT "#pragma once\nnamespace ludork::engine::materials { inline constexpr char MaterialCacheCompilerFingerprint[] = \"${LUDORK_MATERIAL_CACHE_FINGERPRINT}\"; }\n")
target_include_directories(Engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
add_library(EngineBindings OBJECT ${LUDORK_ENGINE_BINDINGS})
add_library(Ludork::Engine ALIAS Engine)
foreach(target IN ITEMS Engine EngineBindings)
    target_link_libraries(${target} PRIVATE Ludork::RuntimeConstants)
    target_compile_definitions(${target} PRIVATE
        LUDORK_ENGINE_EXPORTS=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}")
    target_include_directories(${target}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(${target}
        PUBLIC
            Ludork::Runtime
            SFML::Graphics
            SFML::Audio
            SFML::Network
        PRIVATE
            Ludork::Standard)
endforeach()
ludork_add_core_shared(Engine)
ludork_link_engine_platform(Engine)
if(WIN32)
    target_link_libraries(Engine PRIVATE shell32 ole32)
endif()
ludork_configure_core_library(
    Engine
    "${LUDORK_ENGINE_BINDINGS_TRAITS}")
endif()

if(LUDORK_BUILD_PREVIEW_HOST)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkPreviewHostRuntime.cmake")
    ludork_add_preview_host_runtime(PreviewHostRuntime)
endif()
