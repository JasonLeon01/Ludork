include_guard(GLOBAL)

function(ludork_get_script_tools_dependencies output_variable)
    get_filename_component(script_tools_executable
        "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}" ABSOLUTE)
    get_filename_component(script_tools_directory
        "${script_tools_executable}" DIRECTORY)
    string(SHA256 bundle_key "${script_tools_directory}")
    set(dependencies_property "LUDORK_SCRIPT_TOOLS_DEPENDENCIES_${bundle_key}")
    get_property(dependencies_are_set GLOBAL
        PROPERTY "${dependencies_property}" SET)
    if(NOT dependencies_are_set)
        file(GLOB_RECURSE dependencies LIST_DIRECTORIES false CONFIGURE_DEPENDS
            "${script_tools_directory}/*")
        set_property(GLOBAL PROPERTY "${dependencies_property}" "${dependencies}")
    endif()
    get_property(dependencies GLOBAL PROPERTY "${dependencies_property}")
    set(${output_variable} "${dependencies}" PARENT_SCOPE)
endfunction()
