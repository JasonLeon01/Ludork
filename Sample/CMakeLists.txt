cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Main LANGUAGES C CXX)

set(LUDORK_PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(LUDORK_BUILD_DIRECTORY_NAME "${CMAKE_CURRENT_BINARY_DIR}" NAME)
string(SHA256 LUDORK_BUILD_DIRECTORY_HASH "${CMAKE_CURRENT_BINARY_DIR}")
string(SUBSTRING "${LUDORK_BUILD_DIRECTORY_HASH}" 0 8 LUDORK_BUILD_DIRECTORY_HASH)
set(LUDORK_INTERMEDIATE_DIRECTORY
    "${LUDORK_PROJECT_SOURCE_DIR}/Intermediate/${LUDORK_BUILD_DIRECTORY_NAME}-${LUDORK_BUILD_DIRECTORY_HASH}")

option(
    LUDORK_BUILD_SAMPLE_APPLICATION
    "Build the Sample application and its project-specific runtime"
    ON)
set(LUDORK_DESKTOP_PREVIEW_AVAILABLE ON)
if(CMAKE_CROSSCOMPILING OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS"
   OR CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    set(LUDORK_DESKTOP_PREVIEW_AVAILABLE OFF)
endif()
option(LUDORK_BUILD_UI_PREVIEW_HOST
    "Build the project's desktop UI preview host"
    ${LUDORK_DESKTOP_PREVIEW_AVAILABLE})
if(NOT LUDORK_DESKTOP_PREVIEW_AVAILABLE)
    set(LUDORK_BUILD_UI_PREVIEW_HOST OFF CACHE BOOL
        "Build the project's desktop UI preview host" FORCE)
endif()
set(LUDORK_UI_REGISTRY_PATH "" CACHE FILEPATH
    "Desktop UI registry JSON used to validate cross-compiled project assets")
option(
    LUDORK_SAVE_AS_LDC
    "Encrypt standard save slots as LDC files"
    OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/cmake/LudorkBuild.cmake")

if(LUDORK_BUILD_SAMPLE_APPLICATION AND NOT LUDORK_WITH_LUA)
    message(FATAL_ERROR "The game application requires LUDORK_WITH_LUA=ON.")
endif()

if(ANDROID)
    ludork_require_android_package_contract()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    ludork_require_ohos_package_contract()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/cmake/LudorkLuaSF.cmake")

find_package(Threads REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(LUDORK_PLATFORM "ios")
elseif(WIN32)
    set(LUDORK_PLATFORM "win32")
elseif(APPLE)
    set(LUDORK_PLATFORM "darwin")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" LUDORK_PLATFORM)
endif()

set(LUDORK_ENABLE_FFMPEG OFF)
if(LUDORK_BUILD_SAMPLE_APPLICATION)
    set(LUDORK_PROJECT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Main.proj")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${LUDORK_PROJECT_FILE}")
    if(EXISTS "${LUDORK_PROJECT_FILE}")
        file(READ "${LUDORK_PROJECT_FILE}" LUDORK_PROJECT_JSON)
        string(JSON LUDORK_FFMPEG_TYPE ERROR_VARIABLE LUDORK_FFMPEG_TYPE_ERROR
            TYPE "${LUDORK_PROJECT_JSON}" ffmpeg)
        if(LUDORK_FFMPEG_TYPE_ERROR STREQUAL "NOTFOUND"
            AND LUDORK_FFMPEG_TYPE STREQUAL "BOOLEAN")
            string(JSON LUDORK_FFMPEG_VALUE GET "${LUDORK_PROJECT_JSON}" ffmpeg)
            if(LUDORK_FFMPEG_VALUE)
                set(LUDORK_ENABLE_FFMPEG ON)
            endif()
        endif()
    endif()

    if(LUDORK_ENABLE_FFMPEG)
        add_subdirectory(Engine/cmake/FFmpeg)
    endif()
endif()

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(Engine/ThirdParty/zlib zlib EXCLUDE_FROM_ALL)

add_subdirectory(Engine/Standard)

add_subdirectory(Engine/Runtime)

add_subdirectory(Engine/Source)

if(LUDORK_BUILD_UI_PREVIEW_HOST)
    add_subdirectory(Engine/UiPreviewHost)
endif()

if(LUDORK_BUILD_SAMPLE_APPLICATION)
    include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/cmake/LudorkApplication.cmake")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        include("${CMAKE_CURRENT_SOURCE_DIR}/Engine/cmake/LudorkIosApplication.cmake")
        ludork_configure_ios_application(${LUDORK_APPLICATION_TARGET})
    endif()
endif()

foreach(runtime_target IN ITEMS
    entry
    Main
    lua_cjson
    LudorkStandard
    LudorkRuntime
    Engine
    GlobalCore
    GlobalFunctions
    LuaSF
    LuaSF_lua_shared
    sfml-system
    sfml-window
    sfml-graphics
    sfml-audio
    sfml-network)
    if(TARGET ${runtime_target})
        ludork_enable_release_symbols(${runtime_target})
    endif()
endforeach()
