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
set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)

if(NOT DEFINED SFML_HARMONY_DEVICE_FORM
   OR SFML_HARMONY_DEVICE_FORM STREQUAL "")
    message(FATAL_ERROR
        "SFML_HARMONY_DEVICE_FORM must be passed explicitly by the Ludork packer.")
endif()
string(TOUPPER "${SFML_HARMONY_DEVICE_FORM}" SFML_HARMONY_DEVICE_FORM)
if(NOT SFML_HARMONY_DEVICE_FORM STREQUAL "MOBILE"
   AND NOT SFML_HARMONY_DEVICE_FORM STREQUAL "2IN1")
    message(FATAL_ERROR
        "Unsupported SFML_HARMONY_DEVICE_FORM: ${SFML_HARMONY_DEVICE_FORM}")
endif()
set(SFML_HARMONY_DEVICE_FORM "${SFML_HARMONY_DEVICE_FORM}"
    CACHE STRING "" FORCE)

if(NOT DEFINED SFML_OPENGL_ES)
    message(FATAL_ERROR
        "SFML_OPENGL_ES must be passed explicitly by the Ludork packer.")
endif()
if(SFML_HARMONY_DEVICE_FORM STREQUAL "MOBILE" AND NOT SFML_OPENGL_ES)
    message(FATAL_ERROR
        "The HarmonyOS mobile host requires SFML_OPENGL_ES=ON.")
endif()

if(NOT OHOS_COMPATIBLE_SDK_VERSION STREQUAL "22.0.0")
    message(FATAL_ERROR
        "The Ludork HarmonyOS host requires OHOS_COMPATIBLE_SDK_VERSION=22.")
endif()
if(NOT CMAKE_C_COMPILER_TARGET STREQUAL "__LUDORK_COMPILER_TARGET__"
   OR NOT CMAKE_CXX_COMPILER_TARGET STREQUAL "__LUDORK_COMPILER_TARGET__")
    message(FATAL_ERROR
        "The Ludork HarmonyOS host requires the API 22 native compiler target.")
endif()

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
