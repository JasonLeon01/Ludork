cmake_minimum_required(VERSION 3.21)

foreach(required_variable IN ITEMS
    LUDORK_VS_SOURCE_SOLUTION
    LUDORK_VS_OUTPUT_SOLUTION
    LUDORK_VS_BINARY_DIR)
    if(NOT DEFINED ${required_variable}
        OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT EXISTS "${LUDORK_VS_SOURCE_SOLUTION}")
    message(FATAL_ERROR
        "Visual Studio solution was not found: ${LUDORK_VS_SOURCE_SOLUTION}")
endif()

get_filename_component(
    output_directory
    "${LUDORK_VS_OUTPUT_SOLUTION}"
    DIRECTORY)
file(
    RELATIVE_PATH
    relative_binary_directory
    "${output_directory}"
    "${LUDORK_VS_BINARY_DIR}")
file(TO_NATIVE_PATH "${relative_binary_directory}" relative_binary_directory)

file(READ "${LUDORK_VS_SOURCE_SOLUTION}" exported_solution)
set(project_pattern
    "(Project\\(\"\\{[A-Fa-f0-9-]+\\}\"\\) = \"[^\"]+\", \")([^\"]+\\.vcxproj\")")
string(REGEX MATCHALL "${project_pattern}" solution_projects "${exported_solution}")
list(LENGTH solution_projects project_count)

if(project_count EQUAL 0)
    message(FATAL_ERROR
        "Visual Studio solution contains no C++ projects: ${LUDORK_VS_SOURCE_SOLUTION}")
endif()

set(project_path_marker "__LUDORK_VS_BINARY_DIRECTORY__")
string(
    REGEX REPLACE
    "${project_pattern}"
    "\\1${project_path_marker}\\2"
    exported_solution
    "${exported_solution}")
set(project_path_prefix "${relative_binary_directory}\\")
string(
    REPLACE
    "${project_path_marker}"
    "${project_path_prefix}"
    exported_solution
    "${exported_solution}")

file(WRITE "${LUDORK_VS_OUTPUT_SOLUTION}" "${exported_solution}")
