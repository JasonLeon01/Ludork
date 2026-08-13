include_guard(GLOBAL)

function(ludork_generate_lua_bindings module_name include_directory output_variable)
    set(options)
    set(one_value_args)
    set(multi_value_args TYPE_REGISTRIES)
    cmake_parse_arguments(BINDING
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    file(GLOB_RECURSE module_headers CONFIGURE_DEPENDS
        "${include_directory}/*.hpp")

    set(type_registry_arguments)
    set(type_registry_headers)
    foreach(type_registry IN LISTS BINDING_TYPE_REGISTRIES)
        list(APPEND type_registry_arguments
            --type-registry "${type_registry}")
        string(REGEX REPLACE "^[^=]+=" "" type_registry_directory
            "${type_registry}")
        file(GLOB_RECURSE current_registry_headers CONFIGURE_DEPENDS
            "${type_registry_directory}/*.hpp")
        list(APPEND type_registry_headers ${current_registry_headers})
    endforeach()

    set(bindings "${CMAKE_CURRENT_BINARY_DIR}/${module_name}Bindings.cpp")
    set(stub
        "${LUDORK_CORE_SOURCE_DIR}/../Scripts/stub/${module_name}.d.lua")
    set(metadata_stamp
        "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.metadata.stamp")
    get_filename_component(callback_codecs_directory
        "${LUASF_CALLBACK_CODECS_FILE}" DIRECTORY)
    set(callback_codecs_api "${callback_codecs_directory}/sfml_api.json")

    add_custom_command(
        OUTPUT "${bindings}" "${stub}" "${metadata_stamp}"
        COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            core-bindgen
            --source-root "${LUDORK_CORE_SOURCE_DIR}"
            --include-directory "${include_directory}"
            --module "${module_name}"
            --bindings "${bindings}"
            --stub "${stub}"
            --scripts-directory "${LUDORK_CORE_SOURCE_DIR}/../Scripts"
            --metadata-stamp "${metadata_stamp}"
            --callback-codecs "${LUASF_CALLBACK_CODECS_FILE}"
            ${type_registry_arguments}
        DEPENDS
            ${module_headers}
            ${type_registry_headers}
            "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            "${LUDORK_CORE_SOURCE_DIR}/include/BindAnnotations.hpp"
            "${LUASF_CALLBACK_CODECS_FILE}"
            "${callback_codecs_api}"
        VERBATIM)

    add_custom_target(${module_name}Stub DEPENDS "${bindings}" "${stub}")
    set(${output_variable} "${bindings}" PARENT_SCOPE)
endfunction()
