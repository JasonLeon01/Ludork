cmake_minimum_required(VERSION 3.28)

project(LudorkHarmonyHost LANGUAGES C CXX)

__LUDORK_CMAKE_CONFIG__

if(NOT DEFINED LUDORK_PROJECT_DIR OR LUDORK_PROJECT_DIR STREQUAL "")
    message(FATAL_ERROR "LUDORK_PROJECT_DIR is required for a HarmonyOS build.")
endif()

get_filename_component(
    LUDORK_PROJECT_DIR
    "${LUDORK_PROJECT_DIR}"
    ABSOLUTE)
if(NOT EXISTS "${LUDORK_PROJECT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Ludork project CMakeLists.txt was not found: ${LUDORK_PROJECT_DIR}")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(SFML_CONFIGURE_EXTRAS OFF CACHE BOOL "" FORCE)
set(SFML_HARMONY_DEVICE_FORM MOBILE CACHE STRING "" FORCE)
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)

add_subdirectory("${LUDORK_PROJECT_DIR}" ludork)

if(NOT TARGET entry)
    message(FATAL_ERROR
        "The Ludork project did not create the HarmonyOS entry target.")
endif()

target_sources(entry PRIVATE napi_init.cpp)
target_compile_features(entry PRIVATE cxx_std_20)
target_include_directories(entry PRIVATE
    "${LUDORK_PROJECT_DIR}/include"
    "${LUDORK_PROJECT_DIR}/LuaSF/third_party/SFML/src")
target_link_libraries(entry PRIVATE
    SFML::Main
    ace_napi.z
    hilog_ndk.z)
set_target_properties(entry PROPERTIES OUTPUT_NAME entry)
