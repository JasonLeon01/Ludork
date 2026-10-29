add_executable(PreviewHostExecutable)
add_subdirectory(src)
set_target_properties(PreviewHostExecutable PROPERTIES OUTPUT_NAME PreviewHost)
ludork_set_runtime_output(PreviewHostExecutable)
target_compile_features(PreviewHostExecutable PRIVATE cxx_std_20)
target_link_libraries(PreviewHostExecutable PRIVATE
    Ludork::PreviewHostRuntime Ludork::Video Ludork::RuntimeConstants)
ludork_enable_release_symbols(PreviewHostExecutable)
if(APPLE)
    set_target_properties(PreviewHostExecutable PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@loader_path")
endif()

set(preview_runtime_targets LudorkRuntime LudorkStandard
    sfml-system sfml-window sfml-graphics sfml-audio)
if(LUDORK_WITH_LUA)
    get_target_property(preview_glue_target LuaGlue::LuaGlue ALIASED_TARGET)
    if(NOT preview_glue_target)
        set(preview_glue_target LuaGlue::LuaGlue)
    endif()
    list(APPEND preview_runtime_targets
        LuaSF LuaSF_lua_shared ${preview_glue_target} sfml-network)
endif()
add_custom_target(PreviewPrepare
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        preview begin-build "${LUDORK_PROJECT_SOURCE_DIR}"
        "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
        --configuration "$<CONFIG>"
    WORKING_DIRECTORY "${LUDORK_PROJECT_SOURCE_DIR}"
    VERBATIM)
foreach(runtime_target IN ITEMS PreviewHostExecutable PreviewHostRuntime ${preview_runtime_targets})
    add_dependencies(${runtime_target} PreviewPrepare)
endforeach()
set(preview_runtime_files
    "$<TARGET_FILE:PreviewHostExecutable>"
    "$<TARGET_FILE:PreviewHostRuntime>")
set(preview_runtime_links)
ludork_get_windows_runtime_files(windows_runtime_files)
list(APPEND preview_runtime_files ${windows_runtime_files})
if(LUDORK_ENABLE_FFMPEG)
    list(APPEND preview_runtime_files ${LUDORK_FFMPEG_RUNTIME_FILES})
    ludork_copy_ffmpeg_runtime(PreviewHostExecutable)
endif()
foreach(runtime_target IN LISTS preview_runtime_targets)
    get_target_property(runtime_type ${runtime_target} TYPE)
    if(runtime_type STREQUAL "SHARED_LIBRARY" OR runtime_type STREQUAL "MODULE_LIBRARY")
        list(APPEND preview_runtime_files "$<TARGET_FILE:${runtime_target}>")
        if(APPLE AND runtime_type STREQUAL "SHARED_LIBRARY")
            list(APPEND preview_runtime_links
                "$<TARGET_SONAME_FILE_NAME:${runtime_target}>|$<TARGET_FILE_NAME:${runtime_target}>")
        endif()
    endif()
endforeach()
if(MSVC)
    function(ludork_preview_system_runtime output debug)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
        set(CMAKE_INSTALL_DEBUG_LIBRARIES ${debug})
        set(CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY ${debug})
        set(CMAKE_INSTALL_UCRT_LIBRARIES ${debug})
        include(InstallRequiredSystemLibraries)
        if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
            message(FATAL_ERROR "The compiler runtime files for PreviewHost were not found")
        endif()
        set(${output} "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}" PARENT_SCOPE)
    endfunction()
    ludork_preview_system_runtime(preview_release_crt FALSE)
    if(LUDORK_OPTIMIZE_DEBUG)
        list(APPEND preview_runtime_files ${preview_release_crt})
    else()
        ludork_preview_system_runtime(preview_debug_crt TRUE)
        foreach(runtime_file IN LISTS preview_release_crt)
            list(APPEND preview_runtime_files "$<$<NOT:$<CONFIG:Debug>>:${runtime_file}>")
        endforeach()
        foreach(runtime_file IN LISTS preview_debug_crt)
            list(APPEND preview_runtime_files "$<$<CONFIG:Debug>:${runtime_file}>")
        endforeach()
    endif()
endif()
set(preview_stage_config "${CMAKE_CURRENT_BINARY_DIR}/StageRuntime-$<CONFIG>.cmake")
set(preview_build_info_header "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/PreviewBuildInfoData.hpp")
file(GENERATE OUTPUT "${preview_stage_config}" CONTENT
    "set(PREVIEW_EXECUTABLE [==[$<TARGET_FILE:PreviewHostExecutable>]==])\nset(PREVIEW_RUNTIME_FILES [==[${preview_runtime_files}]==])\nset(PREVIEW_RUNTIME_LINKS [==[${preview_runtime_links}]==])\nset(PREVIEW_PLATFORM [==[${LUDORK_PLATFORM}]==])\nset(PREVIEW_CONFIGURATION [==[$<CONFIG>]==])\nset(PREVIEW_BUILD_INFO_HEADER [==[${preview_build_info_header}]==])\nset(PREVIEW_MACOS ${APPLE})\n")
add_custom_command(OUTPUT "${preview_build_info_header}"
    COMMAND "${CMAKE_COMMAND}"
        "-DPREVIEW_STAGE_CONFIG=${preview_stage_config}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
    DEPENDS
        "${preview_stage_config}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RuntimeFiles.cmake"
    VERBATIM)
target_sources(PreviewHostExecutable PRIVATE "${preview_build_info_header}")
target_include_directories(PreviewHostExecutable PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>")
add_custom_target(PreviewHost
    COMMAND "${CMAKE_COMMAND}"
        "-DPREVIEW_STAGE_CONFIG=${preview_stage_config}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageRuntime.cmake"
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        preview publish "${LUDORK_PROJECT_SOURCE_DIR}"
        "$<TARGET_FILE_DIR:PreviewHostExecutable>"
        --configuration "$<CONFIG>"
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
        ui-assets validate "${LUDORK_PROJECT_SOURCE_DIR}"
    DEPENDS PreviewHostExecutable PreviewHostRuntime ${preview_runtime_targets}
    WORKING_DIRECTORY "${LUDORK_PROJECT_SOURCE_DIR}"
    VERBATIM)
