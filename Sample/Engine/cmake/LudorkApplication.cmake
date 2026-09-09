ludork_add_impl_boundary_validation_target(
    ImplBoundaryValidate
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_custom_target(UiAssetGenerate
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        ui-assets generate "${LUDORK_PROJECT_SOURCE_DIR}"
    WORKING_DIRECTORY "${LUDORK_PROJECT_SOURCE_DIR}"
    VERBATIM)
add_dependencies(ImplBoundaryValidate UiAssetGenerate)
add_dependencies(Engine ImplBoundaryValidate)
if(LUDORK_BUILD_UI_PREVIEW_HOST)
    add_dependencies(Engine UiPreviewHost)
else()
    if(NOT LUDORK_UI_REGISTRY_PATH OR NOT EXISTS "${LUDORK_UI_REGISTRY_PATH}")
        message(FATAL_ERROR "LUDORK_UI_REGISTRY_PATH must identify the desktop project's UI registry JSON")
    endif()
    add_custom_target(UiAssetValidate
        COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            ui-assets validate "${LUDORK_PROJECT_SOURCE_DIR}"
            --registry "${LUDORK_UI_REGISTRY_PATH}"
        WORKING_DIRECTORY "${LUDORK_PROJECT_SOURCE_DIR}"
        VERBATIM)
    add_dependencies(Engine UiAssetValidate)
endif()

set(LUA_CJSON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/lua-cjson")
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
    target_sources(Main PRIVATE
        src/Platform/Android/TextInputHostAndroid.cpp)
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
if(NOT LUDORK_STATIC_LUA_MODULES AND NOT LUDORK_BUILD_UI_PREVIEW_HOST)
    ludork_copy_runtime_libraries(${LUDORK_APPLICATION_TARGET})
    ludork_add_macos_runtime_symlinks(${LUDORK_APPLICATION_TARGET})
endif()
ludork_copy_core_runtime(${LUDORK_APPLICATION_TARGET})
if(LUDORK_ENABLE_FFMPEG AND NOT LUDORK_STATIC_LUA_MODULES)
    ludork_copy_ffmpeg_runtime(${LUDORK_APPLICATION_TARGET})
endif()
