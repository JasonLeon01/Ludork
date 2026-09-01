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
    LUDORK_ENGINE_BINDINGS)

add_library(Engine ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_ENGINE_SOURCES}
    ${LUDORK_ENGINE_BINDINGS})
add_library(Ludork::Engine ALIAS Engine)
ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/ClassDict.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/Graph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/Node.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/BlueprintRuntimeCache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/BlueprintRuntimeDescriptors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/BlueprintRuntimeEvents.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/BlueprintRuntimeExecution.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/BlueprintRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineClassRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineClassRuntimeInstance.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineClassRuntimeMixin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineClassRuntimeResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/MetadataRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeGraphRuntimeInternals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeGraphRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/RuntimeServiceCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/RuntimeServiceInternals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/RuntimeValueServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/DataValue.cpp")
target_compile_definitions(Engine PRIVATE
    LUDORK_ENGINE_EXPORTS=1
    LUDORK_PLATFORM="${LUDORK_PLATFORM}")
target_include_directories(Engine
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(Engine
    PUBLIC
        Ludork::CoreBinding
        SFML::Graphics
        SFML::Audio
        SFML::Network
    PRIVATE
        Ludork::Standard
        zlibstatic)
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
