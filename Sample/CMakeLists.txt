cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Main LANGUAGES C CXX)

option(
    LUDORK_BUILD_SAMPLE_APPLICATION
    "Build the Sample application and its project-specific runtime"
    ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LudorkBuild.cmake")

set(LUASF_LUA_STUB_OUTPUT
    "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/stub/LuaSF.d.lua")
add_subdirectory(LuaSF)
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
   OR CMAKE_SYSTEM_NAME STREQUAL "OHOS")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/LuaSF/third_party/sol2/include")

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

add_subdirectory(Core)

add_custom_target(UiAssetValidate
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        ui-adapter-check
        "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        ui-assets
        validate
        "${CMAKE_CURRENT_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM)
add_dependencies(Engine UiAssetValidate)

if(LUDORK_BUILD_SAMPLE_APPLICATION)
set(LUA_CJSON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lua-cjson")
if(NOT EXISTS "${LUA_CJSON_SOURCE_DIR}/lua_cjson.c")
    message(FATAL_ERROR "lua-cjson source was not found. Run the platform init tool first.")
endif()

add_library(lua_cjson ${LUDORK_MODULE_LIBRARY_TYPE}
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

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    add_library(entry SHARED
        src/Application.cpp
        src/main.cpp)
    set(LUDORK_APPLICATION_TARGET entry)
else()
    add_executable(Main
        src/Application.cpp
        src/main.cpp)
    set(LUDORK_APPLICATION_TARGET Main)
endif()
ludork_configure_visual_studio_play(${LUDORK_APPLICATION_TARGET})
if(TARGET LudorkCacheLuac)
    add_dependencies(${LUDORK_APPLICATION_TARGET} LudorkCacheLuac)
endif()
target_compile_features(${LUDORK_APPLICATION_TARGET} PRIVATE cxx_std_20)
target_include_directories(${LUDORK_APPLICATION_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${LUDORK_APPLICATION_TARGET} PRIVATE
    LuaSF::LuaSF
    LuaSF::Lua
    Ludork::Standard
    Ludork::Engine
    Ludork::Global)
if(LUDORK_STATIC_LUA_MODULES)
    target_compile_definitions(
        ${LUDORK_APPLICATION_TARGET}
        PRIVATE
        LUDORK_STATIC_LUA_MODULES=1)
    target_link_libraries(${LUDORK_APPLICATION_TARGET} PRIVATE
        lua_cjson
        CoreSystem
        GlobalFunctions)
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
        add_custom_command(TARGET Main POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_BUNDLE_DIR:Main>/${resource_directory}"
            COMMAND "${LUDORK_RSYNC_EXECUTABLE}"
                -a
                --delete
                --exclude=.DS_Store
                --exclude=*.anim.json
                "${${resource_source_variable}}/"
                "$<TARGET_BUNDLE_DIR:Main>/${resource_directory}/"
            VERBATIM)
    endforeach()
    add_custom_command(TARGET Main POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_BUNDLE_DIR:Main>/Scripts"
        COMMAND "${LUDORK_RSYNC_EXECUTABLE}"
            -a
            --delete
            --exclude=.DS_Store
            "${LUDORK_SCRIPTS_SOURCE_DIR}/"
            "$<TARGET_BUNDLE_DIR:Main>/Scripts/"
        VERBATIM)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Licenses")
        add_custom_command(TARGET Main POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_BUNDLE_DIR:Main>/Licenses"
            COMMAND "${LUDORK_RSYNC_EXECUTABLE}"
                -a
                --delete
                --exclude=.DS_Store
                "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/"
                "$<TARGET_BUNDLE_DIR:Main>/Licenses/"
            VERBATIM)
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
    Engine
    GlobalCore
    GlobalFunctions
    CoreSystem
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
