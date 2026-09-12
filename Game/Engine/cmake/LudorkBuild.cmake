include_guard(GLOBAL)

option(
    LUDORK_WITH_LUA
    "Build the complete Lua scripting runtime and bindings"
    ON)

option(
    LUDORK_OPTIMIZE_DEBUG
    "Build Debug with release-grade optimization while retaining debug symbols"
    ON)
option(
    LUDORK_DEBUG_SOL_SAFETIES
    "Enable full sol2 safety checks in Debug builds"
    OFF)

if(LUDORK_WITH_LUA)
    add_library(LudorkSolConfig INTERFACE)
    add_library(Ludork::SolConfig ALIAS LudorkSolConfig)
    target_compile_definitions(LudorkSolConfig INTERFACE
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${LUDORK_DEBUG_SOL_SAFETIES}>>:SOL_ALL_SAFETIES_ON=1>
        SOL_SAFE_NUMERICS=1
        SOL_USE_INTEROP=1
        SOL_DEFAULT_AUTOMAGICAL_USERTYPES=0
        SOL_USERTYPE_TYPE_BINDING_INFO=0)
endif()

function(ludork_require_android_package_contract)
    if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR
            "The Android package supports only the arm64-v8a ABI.")
    endif()
    if(NOT ANDROID_PLATFORM_LEVEL STREQUAL "24")
        message(FATAL_ERROR
            "The Android package requires API level 24.")
    endif()
    if(DEFINED CMAKE_ANDROID_STL_TYPE
       AND NOT CMAKE_ANDROID_STL_TYPE STREQUAL "c++_static")
        message(FATAL_ERROR
            "The Android package requires the c++_static runtime.")
    endif()
    if(DEFINED ANDROID_STL AND NOT ANDROID_STL STREQUAL "c++_static")
        message(FATAL_ERROR
            "The Android package requires ANDROID_STL=c++_static.")
    endif()
endfunction()

function(ludork_require_ohos_package_contract)
    if(NOT OHOS_COMPATIBLE_SDK_VERSION STREQUAL "22.0.0")
        message(FATAL_ERROR
            "The HarmonyOS package requires OHOS_COMPATIBLE_SDK_VERSION=22.")
    endif()
    if(NOT CMAKE_C_COMPILER_TARGET STREQUAL "aarch64-linux-ohos22.0.0"
       OR NOT CMAKE_CXX_COMPILER_TARGET STREQUAL "aarch64-linux-ohos22.0.0")
        message(FATAL_ERROR
            "The HarmonyOS package requires the API 22 native compiler target.")
    endif()
endfunction()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS"
   OR CMAKE_SYSTEM_NAME STREQUAL "OHOS"
   OR ANDROID)
    set(LUDORK_STATIC_LUA_MODULES ON)
    set(LUDORK_RUNTIME_LIBRARY_TYPE STATIC)
    set(LUDORK_MODULE_LIBRARY_TYPE STATIC)
else()
    set(LUDORK_STATIC_LUA_MODULES OFF)
    set(LUDORK_RUNTIME_LIBRARY_TYPE SHARED)
    set(LUDORK_MODULE_LIBRARY_TYPE MODULE)
endif()

if(LUDORK_OPTIMIZE_DEBUG)
    add_compile_definitions("$<$<CONFIG:Debug>:NDEBUG>")
    if(MSVC)
        string(REGEX REPLACE "/RTC1|/Ob0" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REGEX REPLACE "/RTC1|/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        add_compile_options(
            "$<$<CONFIG:Debug>:/O2>"
            "$<$<CONFIG:Debug>:/Ob3>")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang"
        OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options("$<$<CONFIG:Debug>:-O3>")
    endif()
endif()

if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "" FORCE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path")
endif()

if(MSVC)
    if(LUDORK_OPTIMIZE_DEBUG)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    add_compile_options(
        /utf-8
        /MP
        /FS
        "$<$<CONFIG:Release>:/experimental:deterministic>"
        "$<$<CONFIG:Release>:/pathmap:${CMAKE_SOURCE_DIR}=.>"
        "$<$<CONFIG:Release>:/pathmap:${CMAKE_BINARY_DIR}=.>")
endif()

function(ludork_apply_msvc_debug_source_options)
    if(NOT MSVC)
        return()
    endif()

    foreach(source IN LISTS ARGN)
        set_property(SOURCE "${source}" APPEND PROPERTY
            COMPILE_OPTIONS
                "$<$<CONFIG:Debug>:/O1>"
                "$<$<CONFIG:Debug>:/Oy->")
        if(NOT LUDORK_OPTIMIZE_DEBUG OR NOT CMAKE_GENERATOR MATCHES "^Visual Studio")
            set_property(SOURCE "${source}" APPEND PROPERTY
                COMPILE_OPTIONS "$<$<CONFIG:Debug>:/Ob1>")
        endif()
        set_property(SOURCE "${source}" APPEND PROPERTY
            VS_SETTINGS
                "$<$<CONFIG:Debug>:BasicRuntimeChecks=Default>")
    endforeach()
endfunction()

if(NOT DEFINED LUDORK_RUNTIME_OUTPUT_DIRECTORY
    OR LUDORK_RUNTIME_OUTPUT_DIRECTORY STREQUAL "")
    if(LUDORK_STATIC_LUA_MODULES)
        set(LUDORK_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    else()
        set(LUDORK_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    endif()
endif()
set(LUDORK_RUNTIME_OUTPUT_DIRECTORY
    "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}"
    CACHE PATH
    "Directory for Ludork runtime binaries")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

function(ludork_set_runtime_output target)
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
endfunction()

function(ludork_enable_release_dead_strip target)
    if(NOT APPLE)
        return()
    endif()

    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "EXECUTABLE"
       OR target_type STREQUAL "SHARED_LIBRARY"
       OR target_type STREQUAL "MODULE_LIBRARY")
        target_link_options(${target} PRIVATE
            "$<$<CONFIG:Release>:LINKER:-dead_strip>")
    endif()
endfunction()

function(ludork_configure_visual_studio_play target)
    if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio")
        return()
    endif()

    set_target_properties(${target} PROPERTIES
        VS_DEBUGGER_COMMAND "$<TARGET_FILE:${target}>"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VS_DEBUGGER_ENVIRONMENT
            "LUDORK_EDITOR=1\nLUDORK_WINDOW_MODE=individual")
    if(NOT DEFINED LUDORK_BUILD_CPP_SCRIPT
        OR LUDORK_BUILD_CPP_SCRIPT STREQUAL "")
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${target})
        return()
    endif()
    if(NOT EXISTS "${LUDORK_BUILD_CPP_SCRIPT}")
        message(FATAL_ERROR
            "Ludork C++ build tool was not found: ${LUDORK_BUILD_CPP_SCRIPT}")
    endif()

    set(ludork_play_source
        "${LUDORK_INTERMEDIATE_DIRECTORY}/LudorkPlay.cpp")
    file(MAKE_DIRECTORY "${LUDORK_INTERMEDIATE_DIRECTORY}")
    file(WRITE "${ludork_play_source}" "int main() { return 0; }\n")
    add_executable(LudorkPlay EXCLUDE_FROM_ALL "${ludork_play_source}")
    add_custom_command(TARGET LudorkPlay PRE_BUILD
        COMMAND "${CMAKE_COMMAND}" -E env
            "PYTHONUTF8=1"
            "PYTHONIOENCODING=utf-8"
            "$ENV{COMSPEC}"
            /d
            /c
            call
            "${LUDORK_BUILD_CPP_SCRIPT}"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            Debug
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT LudorkPlay)
    set_target_properties(LudorkPlay PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/LudorkPlay/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/LudorkPlay/$<CONFIG>"
        VS_DEBUGGER_COMMAND
            "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/Debug/Main.exe"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VS_DEBUGGER_ENVIRONMENT
            "LUDORK_EDITOR=1\nLUDORK_WINDOW_MODE=individual")
endfunction()

function(ludork_add_impl_boundary_validation_target target project_root)
    add_custom_target(${target}
        COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            impl-boundary-check
            "${project_root}"
        WORKING_DIRECTORY "${project_root}"
        VERBATIM)
endfunction()

function(ludork_enable_release_symbols target)
    if(NOT MSVC)
        return()
    endif()

    set_target_properties(${target} PROPERTIES
        PDB_OUTPUT_DIRECTORY "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
    target_link_options(${target} PRIVATE
        "$<$<CONFIG:Release>:/DEBUG:FULL>"
        "$<$<CONFIG:Release>:/PDBALTPATH:%_PDB%>"
        "$<$<CONFIG:Release>:/OPT:REF>"
        "$<$<CONFIG:Release>:/OPT:ICF>")
endfunction()

function(ludork_copy_runtime_libraries target)
    if(LUDORK_WITH_LUA)
        if(NOT COMMAND luasf_copy_runtime_dlls)
            message(FATAL_ERROR "LuaSF runtime copy helper is unavailable.")
        endif()
        luasf_copy_runtime_dlls(${target})
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:sfml-system>"
                "$<TARGET_FILE:sfml-window>"
                "$<TARGET_FILE:sfml-graphics>"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()

function(ludork_add_macos_runtime_symlinks target)
    if(NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()

    set(runtime_targets sfml-system sfml-window sfml-graphics)
    if(LUDORK_WITH_LUA)
        list(APPEND runtime_targets sfml-audio sfml-network)
    endif()
    foreach(runtime_target IN LISTS runtime_targets)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E create_symlink
                "$<TARGET_FILE_NAME:${runtime_target}>"
                "$<TARGET_FILE_DIR:${target}>/$<TARGET_SONAME_FILE_NAME:${runtime_target}>")
    endforeach()
endfunction()
