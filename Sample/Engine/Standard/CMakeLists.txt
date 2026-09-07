cmake_minimum_required(VERSION 3.21)

project(LudorkStandard LANGUAGES CXX)

if(LUDORK_WITH_LUA)
    file(GLOB_RECURSE LUDORK_STANDARD_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    list(FILTER LUDORK_STANDARD_SOURCES EXCLUDE REGEX
        ".*/src/Runtime/ClassRuntime/.*\\.cpp$")
else()
    set(LUDORK_STANDARD_SOURCES
        src/Core/Base64.cpp
        src/Core/Compression.cpp
        src/Core/DataFile.cpp
        src/Core/EncryptedPayload.cpp
        src/Core/Math.cpp
        src/Core/ReadOnlyFileProvider.cpp
        src/Core/Utf8Path.cpp)
endif()

ludork_apply_msvc_debug_source_options(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeConversion.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeDictionary.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeHash.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeIteration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime/ContainerRuntimeSequence.cpp")

add_library(LudorkStandard ${LUDORK_RUNTIME_LIBRARY_TYPE} ${LUDORK_STANDARD_SOURCES})
add_library(Ludork::Standard ALIAS LudorkStandard)
target_compile_features(LudorkStandard PUBLIC cxx_std_20)
target_compile_definitions(LudorkStandard
    PUBLIC LUDORK_WITH_LUA=$<BOOL:${LUDORK_WITH_LUA}>
    PRIVATE
        LUDORK_STANDARD_EXPORTS=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}"
        LUDORK_SAVE_AS_LDC=$<BOOL:${LUDORK_SAVE_AS_LDC}>)
target_include_directories(LudorkStandard
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(LudorkStandard PRIVATE Threads::Threads zlibstatic)
if(LUDORK_WITH_LUA)
    target_include_directories(LudorkStandard
        PUBLIC ${LUDORK_LUASF_CONSUMER_INCLUDES})
    target_link_libraries(LudorkStandard
        PUBLIC Ludork::SolConfig
        PRIVATE LuaSF::LuaSF LuaSF::Lua SFML::Network)
    if(WIN32)
        target_link_libraries(LudorkStandard PRIVATE Psapi)
    endif()
endif()

set_target_properties(LudorkStandard PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF)
ludork_set_runtime_output(LudorkStandard)

if(LUDORK_WITH_LUA)
    add_subdirectory(src/Runtime/ClassRuntime)
    target_link_libraries(LudorkStandard PRIVATE LudorkStandardClassRuntime)
endif()
