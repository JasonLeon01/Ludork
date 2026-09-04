cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Main LANGUAGES C CXX)

option(
    LUDORK_BUILD_SAMPLE_APPLICATION
    "Build the Sample application and its project-specific runtime"
    ON)
option(
    LUDORK_BUILD_UI_PREVIEW_HOST_RUNTIME
    "Build the editor-owned native UI preview runtime"
    OFF)
option(
    LUDORK_SAVE_AS_LDC
    "Encrypt standard save slots as LDC files"
    OFF)
mark_as_advanced(LUDORK_BUILD_UI_PREVIEW_HOST_RUNTIME)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkBuild.cmake")

if(ANDROID)
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
endif()

set(LUASF_LUA_STUB_OUTPUT
    "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/stub/LuaSF.d.lua")
set(
    LUDORK_LUASF_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/LuaSF"
    CACHE PATH
    "LuaSF generated source project used by Ludork")
get_filename_component(
    LUDORK_LUASF_SOURCE_DIR
    "${LUDORK_LUASF_SOURCE_DIR}"
    ABSOLUTE)
if(NOT EXISTS "${LUDORK_LUASF_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "LuaSF source project was not found: ${LUDORK_LUASF_SOURCE_DIR}")
endif()
add_subdirectory("${LUDORK_LUASF_SOURCE_DIR}" LuaSF)
if(TARGET LuaSF_luac AND DEFINED LUDORK_LUAC_CACHE_FILE)
    get_filename_component(
        LUDORK_LUAC_CACHE_DIRECTORY
        "${LUDORK_LUAC_CACHE_FILE}"
        DIRECTORY)
    add_custom_target(LudorkCacheLuac
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${LUDORK_LUAC_CACHE_DIRECTORY}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:LuaSF_luac>"
            "${LUDORK_LUAC_CACHE_FILE}"
        DEPENDS LuaSF_luac
        VERBATIM)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(LuaSF_lua_shared PRIVATE LUA_USE_IOS)
    target_compile_definitions(LuaSF PRIVATE LUASF_IOS_COMPAT=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS"
   OR CMAKE_SYSTEM_NAME STREQUAL "OHOS"
   OR ANDROID)
    foreach(mobile_dependency IN ITEMS
        freetype
        ogg
        FLAC
        vorbis
        vorbisenc
        vorbisfile)
        if(TARGET ${mobile_dependency})
            set_target_properties(${mobile_dependency} PROPERTIES UNITY_BUILD OFF)
        endif()
    endforeach()
endif()

foreach(luasf_binding_target IN ITEMS LuaSF LuaSF_lua_extension)
    if(TARGET ${luasf_binding_target})
        set_property(
            TARGET ${luasf_binding_target}
            PROPERTY INTERPROCEDURAL_OPTIMIZATION_DEBUG
                ${LUDORK_OPTIMIZE_DEBUG})
        if(NOT LUDORK_DEBUG_SOL_SAFETIES)
            get_target_property(
                luasf_compile_definitions
                ${luasf_binding_target}
                COMPILE_DEFINITIONS)
            list(
                REMOVE_ITEM
                luasf_compile_definitions
                "$<$<CONFIG:Debug>:SOL_ALL_SAFETIES_ON=1>")
            set_property(
                TARGET ${luasf_binding_target}
                PROPERTY COMPILE_DEFINITIONS
                    "${luasf_compile_definitions}")
        endif()
    endif()
endforeach()

if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(LuaSF_lua_shared PRIVATE LUA_USE_MACOSX)
endif()

if(NOT TARGET LuaSF::Lua)
    add_library(LuaSF::Lua ALIAS LuaSF_lua_shared)
endif()

set(LUDORK_LUASF_CONSUMER_INCLUDES
    "${CMAKE_CURRENT_BINARY_DIR}/LuaSF/generated_include"
    "${LUDORK_LUASF_SOURCE_DIR}/third_party/sol2/include")

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
        add_subdirectory(cmake/FFmpeg)
    endif()
endif()

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(zlib EXCLUDE_FROM_ALL)

add_subdirectory(Standard)

add_subdirectory(Runtime)

add_subdirectory(Core)

if(LUDORK_BUILD_SAMPLE_APPLICATION)
ludork_add_impl_boundary_validation_target(
    ImplBoundaryValidate
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(Engine ImplBoundaryValidate)
ludork_add_ui_validation_target(
    UiAssetValidate
    "${CMAKE_CURRENT_SOURCE_DIR}"
    VALIDATE_ASSETS)
add_dependencies(Engine UiAssetValidate)

set(LUA_CJSON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua-cjson")
if(NOT EXISTS "${LUA_CJSON_SOURCE_DIR}/lua_cjson.c")
    message(FATAL_ERROR "lua-cjson source was not found. Run the platform init tool first.")
endif()

add_library(lua_cjson ${LUDORK_RUNTIME_LIBRARY_TYPE}
    "${LUA_CJSON_SOURCE_DIR}/lua_cjson.c"
    "${LUA_CJSON_SOURCE_DIR}/strbuf.c"
    "${LUA_CJSON_SOURCE_DIR}/fpconv.c")
if(WIN32)
    target_compile_definitions(lua_cjson PRIVATE
        DISABLE_INVALID_NUMBERS
        strncasecmp=_strnicmp)
    target_link_options(lua_cjson PRIVATE
        /EXPORT:luaopen_cjson
        /EXPORT:luaopen_cjson_safe)
endif()
target_link_libraries(lua_cjson PRIVATE LuaSF::Lua)
set_target_properties(lua_cjson PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cjson")
ludork_set_runtime_output(lua_cjson)

set(LUDORK_APPLICATION_SOURCES
    src/Application.cpp
    src/ApplicationPaths.cpp
    src/ApplicationPaths.hpp
    src/ApplicationPlatform.hpp
    src/ApplicationRuntime.cpp
    src/ApplicationRuntime.hpp
    src/ApplicationScript.cpp
    src/ApplicationStartupError.cpp
    src/main.cpp
    src/Platform/Main.hpp
    src/Platform/MainAndroid.hpp
    src/Platform/MainDefault.hpp
    src/Platform/MainIOS.hpp)

if(LUDORK_STATIC_LUA_MODULES)
    list(APPEND LUDORK_APPLICATION_SOURCES
        src/ApplicationRuntimeModulesStatic.cpp)
else()
    list(APPEND LUDORK_APPLICATION_SOURCES
        src/ApplicationRuntimeModulesDynamic.cpp)
endif()

if(WIN32)
    list(APPEND LUDORK_APPLICATION_SOURCES
        src/Platform/Windows/EmbeddedHost.cpp
        src/Platform/Windows/StartupErrorDisplay.cpp
        src/Platform/Windows/UserDataEnvironment.cpp)
else()
    list(APPEND LUDORK_APPLICATION_SOURCES
        src/Platform/Default/EmbeddedHost.cpp
        src/Platform/Default/StartupErrorDisplay.cpp
        src/Platform/Posix/UserDataEnvironment.cpp)
endif()

if(APPLE)
    list(APPEND LUDORK_APPLICATION_SOURCES
        src/Platform/Apple/BundleResourceRoot.cpp
        src/Platform/Apple/StartupErrorLog.cpp)
else()
    list(APPEND LUDORK_APPLICATION_SOURCES
        src/Platform/Default/BundleResourceRoot.cpp
        src/Platform/Default/StartupErrorLog.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    add_library(entry SHARED ${LUDORK_APPLICATION_SOURCES})
    set(LUDORK_APPLICATION_TARGET entry)
elseif(ANDROID)
    add_library(Main SHARED ${LUDORK_APPLICATION_SOURCES})
    set(LUDORK_APPLICATION_TARGET Main)
else()
    add_executable(Main ${LUDORK_APPLICATION_SOURCES})
    set(LUDORK_APPLICATION_TARGET Main)
endif()
ludork_configure_visual_studio_play(${LUDORK_APPLICATION_TARGET})
if(TARGET LudorkCacheLuac)
    add_dependencies(${LUDORK_APPLICATION_TARGET} LudorkCacheLuac)
endif()
target_compile_features(${LUDORK_APPLICATION_TARGET} PRIVATE cxx_std_20)
target_include_directories(${LUDORK_APPLICATION_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${LUDORK_APPLICATION_TARGET} PRIVATE
    LuaSF::LuaSF
    LuaSF::Lua
    Ludork::Standard
    Ludork::Engine
    Ludork::Global
    lua_cjson)
if(LUDORK_STATIC_LUA_MODULES)
    target_link_libraries(${LUDORK_APPLICATION_TARGET} PRIVATE
        GlobalFunctions)
endif()
if(ANDROID)
    if(NOT DEFINED LUDORK_ANDROID_RUNTIME_HASH
       OR LUDORK_ANDROID_RUNTIME_HASH STREQUAL "")
        message(FATAL_ERROR
            "LUDORK_ANDROID_RUNTIME_HASH is required for an Android build.")
    endif()
    string(LENGTH "${LUDORK_ANDROID_RUNTIME_HASH}"
        LUDORK_ANDROID_RUNTIME_HASH_LENGTH)
    if(NOT LUDORK_ANDROID_RUNTIME_HASH_LENGTH EQUAL 64
       OR NOT LUDORK_ANDROID_RUNTIME_HASH MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "LUDORK_ANDROID_RUNTIME_HASH must be a lowercase SHA-256 hash.")
    endif()
    set_target_properties(Main PROPERTIES OUTPUT_NAME ludork)
    target_compile_definitions(Main PRIVATE
        LUDORK_ANDROID_RUNTIME_HASH=\"${LUDORK_ANDROID_RUNTIME_HASH}\")
    target_link_libraries(Main PRIVATE
        android
        log
        "-Wl,--whole-archive"
        SFML::Main
        "-Wl,--no-whole-archive")
    target_link_options(Main PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384")
endif()
if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(
        ${LUDORK_APPLICATION_TARGET}
        PRIVATE
        "${COREFOUNDATION_FRAMEWORK}")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    foreach(required_variable IN ITEMS
        LUDORK_IOS_APP_NAME
        LUDORK_IOS_BUNDLE_IDENTIFIER
        LUDORK_IOS_DEVELOPMENT_TEAM
        LUDORK_IOS_INFO_PLIST
        LUDORK_IOS_ICON)
        if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
            message(FATAL_ERROR "${required_variable} is required for an iOS build.")
        endif()
    endforeach()
    if(NOT EXISTS "${LUDORK_IOS_INFO_PLIST}")
        message(FATAL_ERROR "iOS Info.plist was not found: ${LUDORK_IOS_INFO_PLIST}")
    endif()
    if(NOT EXISTS "${LUDORK_IOS_ICON}")
        message(FATAL_ERROR "iOS app icon was not found: ${LUDORK_IOS_ICON}")
    endif()
    set(CMAKE_XCODE_GENERATE_SCHEME ON)
    target_sources(Main PRIVATE "${LUDORK_IOS_ICON}")
    set_source_files_properties(
        "${LUDORK_IOS_ICON}"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(Main PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "${LUDORK_IOS_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_NAME "${LUDORK_IOS_APP_NAME}"
        MACOSX_BUNDLE_INFO_PLIST "${LUDORK_IOS_INFO_PLIST}"
        OUTPUT_NAME "${LUDORK_IOS_APP_NAME}"
        XCODE_GENERATE_SCHEME ON
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${LUDORK_IOS_DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${LUDORK_IOS_BUNDLE_IDENTIFIER}"
        XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS iphoneos
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1")
    target_link_libraries(Main PRIVATE
        SFML::Main)
    find_program(LUDORK_RSYNC_EXECUTABLE rsync REQUIRED)
    foreach(resource_directory IN ITEMS Assets Data Scripts)
        string(TOUPPER "${resource_directory}" resource_directory_upper)
        set(resource_source_variable
            "LUDORK_${resource_directory_upper}_SOURCE_DIR")
        if(NOT DEFINED ${resource_source_variable})
            set(${resource_source_variable}
                "${CMAKE_CURRENT_SOURCE_DIR}/${resource_directory}")
        endif()
    endforeach()
    foreach(resource_directory IN ITEMS Assets Data)
        string(TOUPPER "${resource_directory}" resource_directory_upper)
        set(resource_source_variable
            "LUDORK_${resource_directory_upper}_SOURCE_DIR")
        ludork_add_ios_bundle_directory_sync(
            Main
            "${${resource_source_variable}}"
            "${resource_directory}"
            EXCLUDES "*.anim.json")
    endforeach()
    set(
        LUDORK_SCRIPTS_PACKAGE_FILE
        ""
        CACHE FILEPATH
        "Packaged Scripts.ldpak resource used by the iOS application")
    set(ludork_has_loose_scripts OFF)
    if(IS_DIRECTORY "${LUDORK_SCRIPTS_SOURCE_DIR}")
        set(ludork_has_loose_scripts ON)
    endif()
    set(ludork_has_packed_scripts OFF)
    if(NOT LUDORK_SCRIPTS_PACKAGE_FILE STREQUAL "")
        if(NOT EXISTS "${LUDORK_SCRIPTS_PACKAGE_FILE}")
            message(FATAL_ERROR
                "The iOS script package was not found: ${LUDORK_SCRIPTS_PACKAGE_FILE}")
        endif()
        if(IS_DIRECTORY "${LUDORK_SCRIPTS_PACKAGE_FILE}")
            message(FATAL_ERROR
                "The iOS script package is not a file: ${LUDORK_SCRIPTS_PACKAGE_FILE}")
        endif()
        get_filename_component(
            ludork_scripts_package_name
            "${LUDORK_SCRIPTS_PACKAGE_FILE}"
            NAME)
        if(NOT ludork_scripts_package_name STREQUAL "Scripts.ldpak")
            message(FATAL_ERROR
                "The iOS script package must be named Scripts.ldpak: ${LUDORK_SCRIPTS_PACKAGE_FILE}")
        endif()
        set(ludork_has_packed_scripts ON)
    endif()
    if((ludork_has_loose_scripts AND ludork_has_packed_scripts)
       OR (NOT ludork_has_loose_scripts AND NOT ludork_has_packed_scripts))
        message(FATAL_ERROR
            "The iOS application requires exactly one loose Scripts directory or Scripts.ldpak file.")
    endif()
    if(ludork_has_loose_scripts)
        add_custom_command(TARGET Main POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f
                "$<TARGET_BUNDLE_DIR:Main>/Scripts.ldpak"
            VERBATIM)
        ludork_add_ios_bundle_directory_sync(
            Main
            "${LUDORK_SCRIPTS_SOURCE_DIR}"
            Scripts)
    else()
        add_custom_command(TARGET Main POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:Main>/Scripts"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${LUDORK_SCRIPTS_PACKAGE_FILE}"
                "$<TARGET_BUNDLE_DIR:Main>/Scripts.ldpak"
            VERBATIM)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Licenses")
        ludork_add_ios_bundle_directory_sync(
            Main
            "${CMAKE_CURRENT_SOURCE_DIR}/Licenses"
            Licenses)
    endif()
    foreach(legal_file IN ITEMS
        LICENSE.md
        THIRD_PARTY_NOTICES.md
        THIRD_PARTY_NOTICES_zh_CN.md)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${legal_file}")
            add_custom_command(TARGET Main POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/${legal_file}"
                    "$<TARGET_BUNDLE_DIR:Main>/${legal_file}"
                VERBATIM)
        endif()
    endforeach()
    find_program(LUDORK_XATTR_EXECUTABLE xattr REQUIRED)
    add_custom_command(TARGET Main POST_BUILD
        COMMAND "${LUDORK_XATTR_EXECUTABLE}"
            -cr
            "$<TARGET_BUNDLE_DIR:Main>"
        VERBATIM)
endif()
if(WIN32)
    add_executable(LudorkLauncher WIN32 EXCLUDE_FROM_ALL
        src/Platform/Windows/Launcher.cpp
        Main.rc)
    target_compile_features(LudorkLauncher PRIVATE cxx_std_20)
    target_link_libraries(LudorkLauncher PRIVATE User32)
    set_target_properties(LudorkLauncher PROPERTIES
        OUTPUT_NAME Main
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/launcher/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/launcher/$<CONFIG>")
    if(MSVC)
        set_property(TARGET LudorkLauncher PROPERTY
            MSVC_RUNTIME_LIBRARY
                "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(MINGW)
        target_link_options(LudorkLauncher PRIVATE
            -municode
            -static
            -static-libgcc
            -static-libstdc++)
    endif()
    target_sources(Main PRIVATE Main.rc)
    set_property(TARGET Main PROPERTY
        WIN32_EXECUTABLE "$<CONFIG:Release>")
    target_link_libraries(Main PRIVATE
        "$<$<CONFIG:Release>:SFML::Main>")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    ludork_set_runtime_output(${LUDORK_APPLICATION_TARGET})
endif()

add_dependencies(${LUDORK_APPLICATION_TARGET} lua_cjson)
if(NOT LUDORK_STATIC_LUA_MODULES)
    ludork_copy_runtime_libraries(${LUDORK_APPLICATION_TARGET})
    ludork_add_macos_runtime_symlinks(${LUDORK_APPLICATION_TARGET})
endif()
ludork_copy_core_runtime(${LUDORK_APPLICATION_TARGET})
if(LUDORK_ENABLE_FFMPEG AND NOT LUDORK_STATIC_LUA_MODULES)
    ludork_copy_ffmpeg_runtime(${LUDORK_APPLICATION_TARGET})
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
