cmake_minimum_required(VERSION 3.21)

project(LudorkCore LANGUAGES CXX)

set(LUDORK_CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LUDORK_CORE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
get_filename_component(LUDORK_RUNTIME_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../Runtime" ABSOLUTE)
file(REMOVE
    "${LUDORK_PROJECT_SOURCE_DIR}/Scripts/CoreSystem_meta.lua"
    "${LUDORK_PROJECT_SOURCE_DIR}/Scripts/stub/CoreSystem.d.lua")
set(LUDORK_OBSOLETE_CORE_SYSTEM_CONFIGS
    Debug Release RelWithDebInfo MinSizeRel
    ${CMAKE_CONFIGURATION_TYPES}
    ${CMAKE_BUILD_TYPE})
list(REMOVE_DUPLICATES LUDORK_OBSOLETE_CORE_SYSTEM_CONFIGS)
foreach(obsolete_config IN LISTS LUDORK_OBSOLETE_CORE_SYSTEM_CONFIGS)
    if(obsolete_config STREQUAL "")
        continue()
    endif()
    file(REMOVE
        "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/${obsolete_config}/${CMAKE_SHARED_MODULE_PREFIX}CoreSystem${CMAKE_SHARED_MODULE_SUFFIX}"
        "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/${obsolete_config}/CoreSystem.pdb")
endforeach()
set(LUDORK_CORE_RELEASE_LTO ON)
set(LUDORK_CORE_DEBUG_LTO ${LUDORK_OPTIMIZE_DEBUG})
if(MINGW)
    set(LUDORK_CORE_RELEASE_LTO OFF)
    set(LUDORK_CORE_DEBUG_LTO OFF)
endif()
if(NOT DEFINED LUDORK_DEBUG_SOL_SAFETIES)
    set(LUDORK_DEBUG_SOL_SAFETIES ON)
endif()

if(LUDORK_BUILD_SAMPLE_APPLICATION)
set(LUDORK_SCRIPT_TOOLS_EXECUTABLE
    ""
    CACHE FILEPATH
    "Path to the platform ScriptTools executable")
if(NOT LUDORK_SCRIPT_TOOLS_EXECUTABLE OR
   NOT EXISTS "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}")
    message(FATAL_ERROR
        "LUDORK_SCRIPT_TOOLS_EXECUTABLE is missing. Run tools/init first.")
endif()
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

set(LUDORK_BINDING_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LUDORK_BINDING_SCRIPTS_DIRECTORY
    "${LUDORK_PROJECT_SOURCE_DIR}/Scripts")
set(LUDORK_BINDING_ANNOTATIONS_HEADER
    "${LUDORK_RUNTIME_SOURCE_DIR}/include/LudorkRuntimeBinding/Annotations.hpp")
include("${LUDORK_RUNTIME_SOURCE_DIR}/cmake/LudorkBindings.cmake")
ludork_discover_lua_binding_layouts(
    Engine "${CMAKE_CURRENT_SOURCE_DIR}/Core/include"
    Engine "${LUDORK_RUNTIME_SOURCE_DIR}/include/Runtime"
    GlobalCore "${CMAKE_CURRENT_SOURCE_DIR}/Global/include"
    GlobalFunctions "${CMAKE_CURRENT_SOURCE_DIR}/GlobalFunctions/include")

function(ludork_configure_core_library
         target generated_bindings generated_traits)
    add_dependencies(${target} ${target}Stub)
    target_compile_features(${target} PRIVATE cxx_std_20)
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
    foreach(generated_binding IN LISTS generated_bindings)
        set_property(SOURCE "${generated_binding}" PROPERTY
            SKIP_UNITY_BUILD_INCLUSION ON)
    endforeach()
    if(MSVC)
        target_compile_options(${target} PRIVATE "/FI${generated_traits}")
    else()
        target_compile_options(${target} PRIVATE
            -include "${generated_traits}")
    endif()
    if(MSVC)
        foreach(generated_binding IN LISTS generated_bindings)
            set_property(SOURCE "${generated_binding}" APPEND PROPERTY
                COMPILE_OPTIONS /bigobj)
        endforeach()
    endif()
    ludork_set_runtime_output(${target})
endfunction()

add_subdirectory(Core)
add_subdirectory(Global)
add_subdirectory(GlobalFunctions)

add_custom_target(LudorkCoreModules)
add_dependencies(LudorkCoreModules
    Engine GlobalCore GlobalFunctions)

function(ludork_copy_core_runtime target)
    add_dependencies(${target} LudorkCoreModules)
endfunction()
elseif(LUDORK_BUILD_UI_PREVIEW_HOST_RUNTIME)
    add_subdirectory(Core)
endif()
