set(LUASF_LUA_STUB_OUTPUT
    "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/stub/LuaSF.d.lua")
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
