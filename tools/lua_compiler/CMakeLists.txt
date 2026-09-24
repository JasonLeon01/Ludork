cmake_minimum_required(VERSION 3.20)
project(LudorkLuaCompiler LANGUAGES C)

set(LUDORK_LUA_SOURCE_DIR "" CACHE PATH "Lua source directory downloaded from versions.conf")
if(NOT EXISTS "${LUDORK_LUA_SOURCE_DIR}/src/luac.c")
    message(FATAL_ERROR "Lua compiler source was not found: ${LUDORK_LUA_SOURCE_DIR}/src/luac.c")
endif()

file(GLOB lua_sources CONFIGURE_DEPENDS "${LUDORK_LUA_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM lua_sources
    "${LUDORK_LUA_SOURCE_DIR}/src/lua.c"
    "${LUDORK_LUA_SOURCE_DIR}/src/onelua.c"
    "${LUDORK_LUA_SOURCE_DIR}/src/ltests.c")

add_executable(luac ${lua_sources})
target_include_directories(luac PRIVATE "${LUDORK_LUA_SOURCE_DIR}/src")
set_target_properties(luac PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(APPLE)
    target_compile_definitions(luac PRIVATE LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(luac PRIVATE LUA_USE_LINUX)
    target_link_libraries(luac PRIVATE m ${CMAKE_DL_LIBS})
endif()

install(TARGETS luac RUNTIME DESTINATION .)
