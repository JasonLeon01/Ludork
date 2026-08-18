cmake_minimum_required(VERSION 3.21)

project(LudorkCore LANGUAGES CXX)

set(LUDORK_SCRIPT_TOOLS_EXECUTABLE
    ""
    CACHE FILEPATH
    "Path to the platform ScriptTools executable")
if(NOT LUDORK_SCRIPT_TOOLS_EXECUTABLE OR
   NOT EXISTS "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}")
    message(FATAL_ERROR
        "LUDORK_SCRIPT_TOOLS_EXECUTABLE is missing. Run tools/init first.")
endif()

set(LUDORK_CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LUDORK_CORE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
if(NOT DEFINED LUASF_CALLBACK_CODECS_FILE OR
   NOT IS_ABSOLUTE "${LUASF_CALLBACK_CODECS_FILE}" OR
   NOT EXISTS "${LUASF_CALLBACK_CODECS_FILE}")
    message(FATAL_ERROR
        "LuaSF did not export a valid absolute LUASF_CALLBACK_CODECS_FILE.")
endif()
get_filename_component(LUASF_CALLBACK_CODECS_DIRECTORY
    "${LUASF_CALLBACK_CODECS_FILE}" DIRECTORY)
set(LUASF_CALLBACK_CODECS_API_FILE
    "${LUASF_CALLBACK_CODECS_DIRECTORY}/sfml_api.json")
if(NOT EXISTS "${LUASF_CALLBACK_CODECS_API_FILE}")
    message(FATAL_ERROR
        "LuaSF callback codec validation requires sfml_api.json next to the manifest.")
endif()
set(LUDORK_CORE_RELEASE_LTO ON)
set(LUDORK_CORE_DEBUG_LTO ${LUDORK_OPTIMIZE_DEBUG})
if(MINGW)
    set(LUDORK_CORE_RELEASE_LTO OFF)
    set(LUDORK_CORE_DEBUG_LTO OFF)
endif()
if(NOT DEFINED LUDORK_DEBUG_SOL_SAFETIES)
    set(LUDORK_DEBUG_SOL_SAFETIES ON)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkBindings.cmake")

add_library(LudorkCoreBinding INTERFACE)
add_library(Ludork::CoreBinding ALIAS LudorkCoreBinding)
target_compile_features(LudorkCoreBinding INTERFACE cxx_std_20)
target_compile_definitions(LudorkCoreBinding INTERFACE
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${LUDORK_DEBUG_SOL_SAFETIES}>>:SOL_ALL_SAFETIES_ON=1>
    SOL_SAFE_NUMERICS=1
    SOL_USE_INTEROP=1
    SOL_DEFAULT_AUTOMAGICAL_USERTYPES=0
    SOL_USERTYPE_TYPE_BINDING_INFO=0)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    target_compile_definitions(LudorkCoreBinding INTERFACE
        _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()
target_include_directories(LudorkCoreBinding INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${LUDORK_LUASF_CONSUMER_INCLUDES})
target_link_libraries(LudorkCoreBinding INTERFACE
    Ludork::Standard
    LuaSF::LuaSF
    LuaSF::Lua
    SFML::Graphics
    SFML::Audio
    SFML::Network)

function(ludork_configure_core_library target generated_bindings)
    add_dependencies(${target} ${target}Stub)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_link_libraries(${target} PRIVATE Ludork::CoreBinding)
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        INTERPROCEDURAL_OPTIMIZATION_DEBUG ${LUDORK_CORE_DEBUG_LTO}
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LUDORK_CORE_RELEASE_LTO}
        WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    if(NOT LUDORK_STATIC_LUA_MODULES)
        set_target_properties(${target} PROPERTIES
            SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
    endif()
    if(MSVC)
        set_property(SOURCE "${generated_bindings}" APPEND PROPERTY
            COMPILE_OPTIONS /bigobj)
    endif()
    ludork_set_runtime_output(${target})
endfunction()

add_subdirectory(Engine)
add_subdirectory(System)
add_subdirectory(Global)
add_subdirectory(GlobalFunctions)

add_custom_target(LudorkCoreModules)
add_dependencies(LudorkCoreModules
    Engine CoreSystem GlobalCore GlobalFunctions)

function(ludork_copy_core_runtime target)
    add_dependencies(${target} LudorkCoreModules)
endfunction()
