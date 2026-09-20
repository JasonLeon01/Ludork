set(LUASF_GENERATE_LUA_STUB ${LUDORK_WITH_LUA} CACHE BOOL
    "Generate Lua language-server stub from the built LuaSF module" FORCE)
if(LUDORK_WITH_LUA)
    set(LUASF_LUA_STUB_OUTPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/stub/LuaSF.d.lua")
endif()
set(
    LUDORK_LUASF_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/LuaSF"
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
set(
    LUDORK_SFML_SOURCE_DIR
    ""
    CACHE PATH
    "SFML source project used by Ludork and LuaSF; empty uses Engine/ThirdParty/SFML")
if(NOT LUDORK_SFML_SOURCE_DIR)
    set(LUDORK_SFML_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/SFML")
endif()
get_filename_component(
    LUDORK_SFML_SOURCE_DIR
    "${LUDORK_SFML_SOURCE_DIR}"
    ABSOLUTE)
if(NOT EXISTS "${LUDORK_SFML_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "SFML source project was not found: ${LUDORK_SFML_SOURCE_DIR}")
endif()
set(
    LUDORK_LUA_SOURCE_DIR
    ""
    CACHE PATH
    "Lua source directory used by LuaSF; empty uses Engine/ThirdParty/Lua")
if(NOT LUDORK_LUA_SOURCE_DIR)
    set(LUDORK_LUA_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/Lua")
endif()
get_filename_component(
    LUDORK_LUA_SOURCE_DIR
    "${LUDORK_LUA_SOURCE_DIR}"
    ABSOLUTE)
if(NOT EXISTS "${LUDORK_LUA_SOURCE_DIR}/src/lua.h")
    message(FATAL_ERROR
        "Lua source directory was not found: ${LUDORK_LUA_SOURCE_DIR}")
endif()
set(LUDORK_LUAGLUE_SOURCE_DIR "" CACHE PATH "LuaGlue source project; empty uses the LuaSF sibling directory")
if(NOT LUDORK_LUAGLUE_SOURCE_DIR)
    get_filename_component(LUDORK_LUAGLUE_SOURCE_DIR "${LUDORK_LUASF_SOURCE_DIR}/../LuaGlue" ABSOLUTE)
endif()
if(NOT EXISTS "${LUDORK_LUAGLUE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "LuaGlue source project was not found: ${LUDORK_LUAGLUE_SOURCE_DIR}")
endif()

set(LUASF_SFML_ROOT "${LUDORK_SFML_SOURCE_DIR}" CACHE PATH
    "External SFML source project used by LuaSF" FORCE)
set(LUASF_LUA_ROOT "${LUDORK_LUA_SOURCE_DIR}" CACHE PATH
    "External Lua source directory used by LuaSF" FORCE)
set(LUASF_GLUE_ROOT "${LUDORK_LUAGLUE_SOURCE_DIR}" CACHE PATH
    "LuaGlue source project used by LuaSF" FORCE)

function(ludork_add_external_sfml)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS"
       OR CMAKE_SYSTEM_NAME STREQUAL "Android"
       OR CMAKE_SYSTEM_NAME STREQUAL "OHOS")
        set(BUILD_SHARED_LIBS OFF)
    elseif(DEFINED LUASF_BUILD_SHARED_SFML)
        set(BUILD_SHARED_LIBS ${LUASF_BUILD_SHARED_SFML})
    else()
        set(BUILD_SHARED_LIBS ON)
    endif()
    set(SFML_BUILD_EXAMPLES OFF)
    set(SFML_BUILD_DOC OFF)
    set(SFML_BUILD_TEST_SUITE OFF)
    add_subdirectory("${LUDORK_SFML_SOURCE_DIR}" SFML)
endfunction()
ludork_add_external_sfml()

if(LUDORK_WITH_LUA)
    add_subdirectory("${LUDORK_LUASF_SOURCE_DIR}" LuaSF)
else()
    add_subdirectory("${LUDORK_LUASF_SOURCE_DIR}" LuaSF EXCLUDE_FROM_ALL)
endif()

if(MSVC)
    foreach(dependency IN ITEMS
        freetype
        harfbuzz
        SheenBidi
        ogg
        FLAC
        vorbis
        vorbisenc
        vorbisfile
        libssh2_static)
        if(TARGET ${dependency})
            target_compile_options(${dependency} PRIVATE /W0)
        endif()
    endforeach()
endif()

if(NOT LUDORK_WITH_LUA)
    return()
endif()

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

if(TARGET LuaSF)
    set_property(
        TARGET LuaSF
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_DEBUG
            ${LUDORK_OPTIMIZE_DEBUG})
endif()

# Binding objects and their shared runtime follow the host debug policy.
foreach(binding_target IN ITEMS LuaSF_bindings LuaGlue)
    if(TARGET ${binding_target})
        set_property(TARGET ${binding_target} PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_DEBUG ${LUDORK_OPTIMIZE_DEBUG})
    endif()
endforeach()

if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(LuaSF_lua_shared PRIVATE LUA_USE_MACOSX)
endif()

if(NOT TARGET LuaSF::Lua)
    add_library(LuaSF::Lua ALIAS LuaSF_lua_shared)
endif()
