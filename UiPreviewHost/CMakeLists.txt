cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(UiPreviewHost LANGUAGES C CXX)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "UiPreviewHost is available only for desktop platforms.")
endif()

get_filename_component(
    LUDORK_REPOSITORY_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    ABSOLUTE)
set(LUDORK_SAMPLE_SOURCE_DIR "${LUDORK_REPOSITORY_ROOT}/Sample")
set(LUDORK_BUILD_SAMPLE_APPLICATION
    OFF
    CACHE BOOL
    "Build the Sample application and its project-specific runtime"
    FORCE)
set(LUDORK_RUNTIME_OUTPUT_DIRECTORY
    "${LUDORK_REPOSITORY_ROOT}/.tools/UiPreviewHost/bin"
    CACHE PATH
    "Directory for UiPreviewHost runtime binaries"
    FORCE)

include("${LUDORK_SAMPLE_SOURCE_DIR}/cmake/LudorkBuild.cmake")

add_subdirectory(
    "${LUDORK_SAMPLE_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/Sample"
    EXCLUDE_FROM_ALL)

add_custom_target(UiPreviewHostAdapterValidate
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        ui-adapter-check
        "${LUDORK_REPOSITORY_ROOT}"
    WORKING_DIRECTORY "${LUDORK_REPOSITORY_ROOT}"
    VERBATIM)

add_executable(UiPreviewHost
    src/UiPreviewHost.cpp
    src/UiPreviewCurveResolver.cpp)
add_dependencies(UiPreviewHost UiPreviewHostAdapterValidate)
target_compile_features(UiPreviewHost PRIVATE cxx_std_20)
target_link_libraries(UiPreviewHost PRIVATE Ludork::Engine)
ludork_set_runtime_output(UiPreviewHost)
ludork_enable_release_symbols(UiPreviewHost)
ludork_copy_runtime_libraries(UiPreviewHost)
ludork_add_macos_runtime_symlinks(UiPreviewHost)
