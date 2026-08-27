include_guard(GLOBAL)

option(
    LUDORK_OPTIMIZE_DEBUG
    "Build Debug with release-grade optimization while retaining debug symbols"
    ON)
option(
    LUDORK_DEBUG_SOL_SAFETIES
    "Enable full sol2 safety checks in Debug builds"
    OFF)

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
        string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
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
                "$<$<CONFIG:Debug>:/Ob1>"
                "$<$<CONFIG:Debug>:/Oy->")
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
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/LudorkPlay.cpp")
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

function(ludork_add_ui_validation_target target project_root)
    set(options VALIDATE_ASSETS)
    cmake_parse_arguments(UI_VALIDATION
        "${options}"
        ""
        ""
        ${ARGN})

    set(validation_commands
        COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            ui-adapter-check
            "${project_root}")
    if(UI_VALIDATION_VALIDATE_ASSETS)
        list(APPEND validation_commands
            COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
                ui-assets
                validate
                "${project_root}")
    endif()

    add_custom_target(${target}
        ${validation_commands}
        WORKING_DIRECTORY "${project_root}"
        VERBATIM)
endfunction()

function(ludork_add_ios_bundle_directory_sync
    target source_directory bundle_subdirectory)
    set(multi_value_args EXCLUDES)
    cmake_parse_arguments(BUNDLE_SYNC
        ""
        ""
        "${multi_value_args}"
        ${ARGN})

    set(exclude_arguments --exclude=.DS_Store)
    foreach(exclude_pattern IN LISTS BUNDLE_SYNC_EXCLUDES)
        list(APPEND exclude_arguments "--exclude=${exclude_pattern}")
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_BUNDLE_DIR:${target}>/${bundle_subdirectory}"
        COMMAND "${LUDORK_RSYNC_EXECUTABLE}"
            -a
            --delete
            ${exclude_arguments}
            "${source_directory}/"
            "$<TARGET_BUNDLE_DIR:${target}>/${bundle_subdirectory}/"
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
    if(NOT COMMAND luasf_copy_runtime_dlls)
        message(FATAL_ERROR "LuaSF runtime copy helper is unavailable.")
    endif()
    luasf_copy_runtime_dlls(${target})
endfunction()

function(ludork_add_macos_runtime_symlinks target)
    if(NOT APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        return()
    endif()

    foreach(runtime_target IN ITEMS
        sfml-system
        sfml-window
        sfml-graphics
        sfml-audio
        sfml-network)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E create_symlink
                "$<TARGET_FILE_NAME:${runtime_target}>"
                "$<TARGET_FILE_DIR:${target}>/$<TARGET_SONAME_FILE_NAME:${runtime_target}>")
    endforeach()
endfunction()
