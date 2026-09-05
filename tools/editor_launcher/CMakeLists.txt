cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED LUDORK_VERSION OR LUDORK_VERSION STREQUAL "")
    message(FATAL_ERROR "Pass the editor version with -DLUDORK_VERSION=<version>.")
endif()

project(LudorkEditorLauncher VERSION "${LUDORK_VERSION}" LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The editor launcher requires a Windows toolchain.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The editor launcher requires a Windows x64 toolchain.")
endif()

enable_language(RC)

foreach(component MAJOR MINOR PATCH TWEAK)
    if("${PROJECT_VERSION_${component}}" STREQUAL "")
        set(PROJECT_VERSION_${component} 0)
    endif()
    if(PROJECT_VERSION_${component} GREATER 65535)
        message(FATAL_ERROR "The editor version components must not exceed 65535.")
    endif()
endforeach()

get_filename_component(LUDORK_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Assets/icon.ico" ABSOLUTE)
configure_file(Launcher.rc.in Launcher.rc @ONLY)

add_executable(LudorkEditorLauncher WIN32
    Launcher.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/Launcher.rc")
target_compile_features(LudorkEditorLauncher PRIVATE cxx_std_20)
target_compile_definitions(LudorkEditorLauncher PRIVATE
    UNICODE _UNICODE _WIN32_WINNT=0x0A00)
target_link_libraries(LudorkEditorLauncher PRIVATE User32)
set_target_properties(LudorkEditorLauncher PROPERTIES
    OUTPUT_NAME Ludork
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>")

if(MSVC)
    set_property(TARGET LudorkEditorLauncher PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
    target_link_options(LudorkEditorLauncher PRIVATE
        -municode -static -static-libgcc -static-libstdc++)
endif()
