cmake_minimum_required(VERSION 3.21)

project(LudorkEngine LANGUAGES CXX)

file(GLOB_RECURSE LUDORK_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    list(APPEND LUDORK_ENGINE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Input/PlatformInputBridgeMac.mm")
endif()

ludork_generate_lua_bindings(
    Engine
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    LUDORK_ENGINE_BINDINGS)

add_library(Engine ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_ENGINE_SOURCES}
    "${LUDORK_ENGINE_BINDINGS}")
add_library(Ludork::Engine ALIAS Engine)
ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/ClassDict.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/Graph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/Node.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/BlueprintRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineClassRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/EngineRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/MetadataRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/NodeGraphRuntimeServices.cpp"
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
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    target_link_libraries(Engine PRIVATE
        "${APPKIT_FRAMEWORK}"
        "${COREGRAPHICS_FRAMEWORK}")
endif()
ludork_configure_core_library(Engine "${LUDORK_ENGINE_BINDINGS}")
