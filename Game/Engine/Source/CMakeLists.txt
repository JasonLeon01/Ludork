cmake_minimum_required(VERSION 3.21)

project(LudorkCore LANGUAGES CXX)

add_subdirectory(Media)

set(LUDORK_CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
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

function(ludork_configure_core_compilation target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        INTERPROCEDURAL_OPTIMIZATION_DEBUG ${LUDORK_CORE_DEBUG_LTO}
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LUDORK_CORE_RELEASE_LTO})
endfunction()

function(ludork_configure_core_target target)
    ludork_configure_core_compilation(${target})
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    ludork_enable_release_dead_strip(${target})
    ludork_set_runtime_output(${target})
    ludork_target_precompile_headers(${target})
endfunction()

if(LUDORK_BUILD_GAME_APPLICATION)
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

ludork_get_tool(LUDORK_TOOL_CODEGEN codegen)

add_custom_target(LudorkSfEnumModules
    COMMAND "${LUDORK_TOOL_CODEGEN}" enum-modules
        --sfml-api "${LUASF_CALLBACK_CODECS_API_FILE}"
        --module sf
        --scripts-directory "${LUDORK_BINDING_SCRIPTS_DIRECTORY}"
    DEPENDS "${LUASF_CALLBACK_CODECS_API_FILE}"
    VERBATIM)

if(LUDORK_WITH_LUA AND NOT CMAKE_CROSSCOMPILING AND NOT LUDORK_STATIC_LUA_MODULES)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../Tools/NativeStubDump"
        "${CMAKE_CURRENT_BINARY_DIR}/NativeStubDump")
endif()

function(ludork_configure_core_library target generated_traits)
    ludork_configure_core_target(${target})
    set(bindings_target ${target}Bindings)
    ludork_configure_core_compilation(${bindings_target})
    add_dependencies(${bindings_target} ${target}Stub)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:${bindings_target}>)
    target_precompile_headers(${bindings_target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${generated_traits}>")
    set(pch_disabled
        "$<BOOL:$<TARGET_PROPERTY:${bindings_target},DISABLE_PRECOMPILE_HEADERS>>")
    add_dependencies(${target} ${target}Stub)
    if(TARGET LudorkNativeStubDump)
        add_dependencies(${target} LudorkNativeStubDump)
        ludork_copy_runtime_libraries(${target})
        ludork_add_macos_runtime_symlinks(${target})
        if(LUDORK_ENABLE_FFMPEG AND COMMAND ludork_copy_ffmpeg_runtime)
            ludork_copy_ffmpeg_runtime(${target})
        endif()
        set(enum_catalogue
            "${LUDORK_INTERMEDIATE_DIRECTORY}/${target}/${target}.enums.json")
        set(enum_module_commands
            COMMAND "$<TARGET_FILE:LudorkNativeStubDump>"
                "$<TARGET_FILE:${target}>"
                "${target}_write_enum_catalogue"
                "${enum_catalogue}"
            COMMAND "${LUDORK_TOOL_CODEGEN}" enum-modules
                --catalogue "${enum_catalogue}"
                --module "${target}"
                --scripts-directory "${LUDORK_BINDING_SCRIPTS_DIRECTORY}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<TARGET_FILE:LudorkNativeStubDump>"
                "$<TARGET_FILE:${target}>"
                "${target}_write_stub"
                "${LUDORK_BINDING_SCRIPTS_DIRECTORY}/stub/${target}.d.lua"
            ${enum_module_commands}
            VERBATIM)
        add_custom_target(${target}NativeStub
            COMMAND "$<TARGET_FILE:LudorkNativeStubDump>"
                "$<TARGET_FILE:${target}>"
                "${target}_write_stub"
                "${LUDORK_BINDING_SCRIPTS_DIRECTORY}/stub/${target}.d.lua"
            ${enum_module_commands}
            DEPENDS ${target} LudorkNativeStubDump
            VERBATIM)
    endif()
    if(NOT LUDORK_STATIC_LUA_MODULES)
        set_target_properties(${target} PROPERTIES
            SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
    endif()
    set_target_properties(${bindings_target} PROPERTIES UNITY_BUILD OFF)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin"
       AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64"
           OR (NOT CMAKE_OSX_ARCHITECTURES
               AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64"))
            target_compile_options(${bindings_target} PRIVATE
                "$<$<CONFIG:Release>:-Os>")
        endif()
    endif()
    if(MSVC)
        target_compile_options(${bindings_target} PRIVATE
            "$<${pch_disabled}:/FI${generated_traits}>"
            /bigobj)
    else()
        target_compile_options(${bindings_target} PRIVATE
            "$<${pch_disabled}:-include;${generated_traits}>")
    endif()
endfunction()

add_subdirectory(Core)
add_subdirectory(Global)
add_subdirectory(GlobalFunctions)

add_custom_target(LudorkCoreModules)
add_dependencies(LudorkCoreModules
    Engine GlobalCore GlobalFunctions LudorkSfEnumModules)
if(TARGET LudorkNativeStubDump)
    add_dependencies(LudorkCoreModules
        EngineNativeStub GlobalCoreNativeStub GlobalFunctionsNativeStub)
endif()

function(ludork_copy_core_runtime target)
    add_dependencies(${target} LudorkCoreModules)
endfunction()
elseif(LUDORK_BUILD_PREVIEW_HOST)
    add_subdirectory(Core)
endif()
