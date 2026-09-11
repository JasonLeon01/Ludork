cmake_minimum_required(VERSION 3.21)

project(LudorkRuntime LANGUAGES CXX)

if(LUDORK_WITH_LUA)
    file(GLOB_RECURSE LUDORK_RUNTIME_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    list(FILTER LUDORK_RUNTIME_SOURCES EXCLUDE REGEX
        ".*/src/(LuaServices|Metadata|Graphics)/.*\\.cpp$")
else()
    set(LUDORK_RUNTIME_SOURCES
        src/RuntimeData.cpp
        src/RuntimeValue.cpp
        src/RuntimeObject.cpp
        src/RuntimeIdentity.cpp
        src/Json.cpp
        src/AssetPath.cpp
        src/AssetStore.cpp
        src/AssetInputStream.cpp
        src/ResourceStorePaths.cpp
        src/LdPakArchive.cpp)
endif()

ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/MetadataRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeProviders.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeProviderFacade.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeProviderInternals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeReflection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/LuaServices/RuntimeServiceCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/LuaServices/RuntimeServiceInternals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/ClassDict.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/Graph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/Node.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/BlueprintRuntime/BlueprintRuntimeDescriptors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/BlueprintRuntime/BlueprintRuntimeEvents.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/BlueprintRuntime/BlueprintRuntimeExecution.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/BlueprintRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Components/ComponentRuntimeCache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/ClassRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/ClassRuntime/ClassRuntimeInstance.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/ClassRuntime/ClassRuntimeMixin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Blueprint/ClassRuntime/ClassRuntimeResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/NodeGraphRuntime/NodeGraphRuntimeInternals.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NodeGraph/NodeGraphRuntimeServices.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TypedDataService.cpp")

add_library(LudorkRuntime ${LUDORK_RUNTIME_LIBRARY_TYPE}
    ${LUDORK_RUNTIME_SOURCES})
add_library(Ludork::Runtime ALIAS LudorkRuntime)

add_subdirectory(src/Graphics)

if(LUDORK_WITH_LUA)
    add_subdirectory(src/Metadata)
    add_subdirectory(src/LuaServices)
endif()

target_compile_features(LudorkRuntime PUBLIC cxx_std_20)
target_compile_definitions(LudorkRuntime
    PUBLIC LUDORK_WITH_LUA=$<BOOL:${LUDORK_WITH_LUA}>
    PRIVATE LUDORK_RUNTIME_EXPORTS=1)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    target_compile_definitions(LudorkRuntime PUBLIC
        _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()
target_include_directories(LudorkRuntime
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(LudorkRuntime
    PUBLIC Ludork::Standard SFML::Graphics
    PRIVATE zlibstatic)
if(LUDORK_WITH_LUA)
    target_include_directories(LudorkRuntime
        PUBLIC ${LUDORK_LUASF_CONSUMER_INCLUDES})
    target_link_libraries(LudorkRuntime
        PUBLIC Ludork::SolConfig LuaSF::LuaSF LuaSF::Lua SFML::Audio SFML::Network)
endif()

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
