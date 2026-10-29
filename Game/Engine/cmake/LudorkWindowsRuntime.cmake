include_guard(GLOBAL)

if(MINGW)
    get_filename_component(ludork_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(runtime_name IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        set(runtime_file "${ludork_compiler_directory}/${runtime_name}")
        if(NOT EXISTS "${runtime_file}")
            message(FATAL_ERROR "MinGW runtime dependency was not found: ${runtime_file}")
        endif()
        set_property(GLOBAL APPEND PROPERTY LUDORK_WINDOWS_RUNTIME_FILES "${runtime_file}")
    endforeach()
endif()

function(ludork_get_windows_runtime_files output)
    get_property(runtime_files GLOBAL PROPERTY LUDORK_WINDOWS_RUNTIME_FILES)
    set(${output} "${runtime_files}" PARENT_SCOPE)
endfunction()

function(ludork_copy_windows_runtime_libraries target)
    ludork_get_windows_runtime_files(runtime_files)
    if(runtime_files)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${runtime_files}
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endif()
endfunction()
