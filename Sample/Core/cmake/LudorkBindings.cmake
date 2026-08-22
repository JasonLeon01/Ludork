include_guard(GLOBAL)

function(ludork_discover_lua_binding_layouts)
    math(EXPR argument_remainder "${ARGC} % 2")
    if(ARGC EQUAL 0 OR NOT argument_remainder EQUAL 0)
        message(FATAL_ERROR
            "ludork_discover_lua_binding_layouts expects MODULE INCLUDE_DIRECTORY pairs.")
    endif()

    set(layout_arguments)
    set(layout_dependencies "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}")
    set(module_names)
    set(argument_index 0)
    while(argument_index LESS ARGC)
        list(GET ARGV ${argument_index} module_name)
        math(EXPR include_directory_index "${argument_index} + 1")
        list(GET ARGV ${include_directory_index} include_directory)
        if(NOT module_name MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
            message(FATAL_ERROR
                "Invalid Lua binding layout module name: ${module_name}")
        endif()
        if(NOT IS_DIRECTORY "${include_directory}")
            message(FATAL_ERROR
                "Lua binding include directory does not exist: "
                "${include_directory}")
        endif()
        list(FIND module_names "${module_name}" existing_module_index)
        if(NOT existing_module_index EQUAL -1)
            message(FATAL_ERROR
                "Duplicate Lua binding layout module: ${module_name}")
        endif()
        list(APPEND module_names "${module_name}")
        list(APPEND layout_arguments
            --module "${module_name}" "${include_directory}")
        file(GLOB_RECURSE module_layout_headers CONFIGURE_DEPENDS
            "${include_directory}/*.hpp")
        list(APPEND layout_dependencies ${module_layout_headers})
        math(EXPR argument_index "${argument_index} + 2")
    endwhile()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${layout_dependencies})
    execute_process(
        COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            core-bindgen-layout
            ${layout_arguments}
        RESULT_VARIABLE layout_result
        OUTPUT_VARIABLE layout_json
        ERROR_VARIABLE layout_error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT layout_result EQUAL 0)
        string(STRIP "${layout_error}" layout_error)
        message(FATAL_ERROR
            "Core binding layout discovery failed:\n${layout_error}")
    endif()

    string(JSON layout_schema GET "${layout_json}" schema)
    if(NOT layout_schema STREQUAL "ludork-core-bindgen-layout-v1")
        message(FATAL_ERROR
            "Unsupported Core binding layout schema: ${layout_schema}")
    endif()
    string(JSON discovered_module_count LENGTH "${layout_json}" modules)
    list(LENGTH module_names expected_module_count)
    if(NOT discovered_module_count EQUAL expected_module_count)
        message(FATAL_ERROR
            "Core binding layout discovery returned an unexpected "
            "module count.")
    endif()

    foreach(module_name IN LISTS module_names)
        set(relative_binding_sources)
        string(JSON class_source_count LENGTH "${layout_json}"
            modules "${module_name}" classSources)
        if(class_source_count GREATER 0)
            math(EXPR last_class_source_index "${class_source_count} - 1")
            foreach(class_source_index RANGE 0 ${last_class_source_index})
                string(JSON class_source GET "${layout_json}"
                    modules "${module_name}" classSources
                    ${class_source_index})
                if(NOT class_source MATCHES
                   "^${module_name}\\.[A-Za-z_][A-Za-z0-9_]*\\.auto\\.cpp$" OR
                   class_source STREQUAL "${module_name}.stub.auto.cpp")
                    message(FATAL_ERROR
                        "Invalid generated class binding source: ${class_source}")
                endif()
                list(FIND relative_binding_sources "${class_source}"
                    existing_source_index)
                if(NOT existing_source_index EQUAL -1)
                    message(FATAL_ERROR
                        "Duplicate generated binding source: ${class_source}")
                endif()
                list(APPEND relative_binding_sources "${class_source}")
            endforeach()
        endif()
        string(JSON stub_source GET "${layout_json}"
            modules "${module_name}" stubSource)
        if(NOT stub_source STREQUAL "${module_name}.stub.auto.cpp")
            message(FATAL_ERROR
                "Invalid generated stub binding source: ${stub_source}")
        endif()
        list(APPEND relative_binding_sources "${stub_source}")
        set_property(GLOBAL
            PROPERTY "LUDORK_CORE_BINDING_LAYOUT_${module_name}"
            "${relative_binding_sources}")
    endforeach()
endfunction()

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

    set(generated_scripts_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated-declarations")
    set(generated_bindings_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated-bindings")
    file(REMOVE
        "${CMAKE_CURRENT_BINARY_DIR}/${module_name}Bindings.cpp")
    set(published_scripts_directory
        "${LUDORK_CORE_SOURCE_DIR}/../Scripts")
    set(binding_layout_property
        "LUDORK_CORE_BINDING_LAYOUT_${module_name}")
    get_property(binding_layout_is_set GLOBAL
        PROPERTY "${binding_layout_property}" SET)
    if(NOT binding_layout_is_set)
        message(FATAL_ERROR
            "Lua binding layout was not discovered for ${module_name}.")
    endif()
    get_property(relative_binding_sources GLOBAL
        PROPERTY "${binding_layout_property}")
    set(bindings)
    foreach(relative_binding_source IN LISTS relative_binding_sources)
        list(APPEND bindings
            "${generated_bindings_directory}/${relative_binding_source}")
    endforeach()
    set(bindings_stamp
        "${generated_bindings_directory}/${module_name}.bindings.stamp")
    set(bindings_manifest
        "${generated_bindings_directory}/${module_name}.bindings.manifest")
    set(binding_input_lines "module:${module_name}")
    foreach(relative_binding_source IN LISTS relative_binding_sources)
        list(APPEND binding_input_lines
            "source:${relative_binding_source}")
    endforeach()
    list(SORT module_headers)
    foreach(module_header IN LISTS module_headers)
        list(APPEND binding_input_lines "header:${module_header}")
    endforeach()
    foreach(type_registry IN LISTS BINDING_TYPE_REGISTRIES)
        list(APPEND binding_input_lines "registry:${type_registry}")
    endforeach()
    list(SORT type_registry_headers)
    foreach(type_registry_header IN LISTS type_registry_headers)
        list(APPEND binding_input_lines
            "registry-header:${type_registry_header}")
    endforeach()
    string(JOIN "\n" binding_inputs_content ${binding_input_lines})
    set(binding_inputs
        "${generated_bindings_directory}/${module_name}.inputs")
    file(MAKE_DIRECTORY "${generated_bindings_directory}")
    file(GENERATE
        OUTPUT "${binding_inputs}"
        CONTENT "${binding_inputs_content}\n")
    set(stub
        "${generated_scripts_directory}/stub/${module_name}.d.lua")
    set(metadata
        "${generated_scripts_directory}/${module_name}_meta.lua")
    set(metadata_stamp
        "${generated_scripts_directory}/${module_name}.metadata.stamp")
    set(published_stub
        "${published_scripts_directory}/stub/${module_name}.d.lua")
    set(published_metadata
        "${published_scripts_directory}/${module_name}_meta.lua")
    get_filename_component(callback_codecs_directory
        "${LUASF_CALLBACK_CODECS_FILE}" DIRECTORY)
    set(callback_codecs_api "${callback_codecs_directory}/sfml_api.json")

    add_custom_command(
        OUTPUT "${bindings_stamp}"
        BYPRODUCTS
            ${bindings}
            "${stub}"
            "${metadata}"
            "${metadata_stamp}"
        COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            core-bindgen
            --source-root "${LUDORK_CORE_SOURCE_DIR}"
            --include-directory "${include_directory}"
            --module "${module_name}"
            --bindings-directory "${generated_bindings_directory}"
            --bindings-manifest "${bindings_manifest}"
            --bindings-stamp "${bindings_stamp}"
            --stub "${stub}"
            --scripts-directory "${generated_scripts_directory}"
            --metadata-stamp "${metadata_stamp}"
            --callback-codecs "${LUASF_CALLBACK_CODECS_FILE}"
            ${type_registry_arguments}
        DEPENDS
            ${module_headers}
            ${type_registry_headers}
            "${binding_inputs}"
            "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}"
            "${LUDORK_CORE_SOURCE_DIR}/include/BindAnnotations.hpp"
            "${LUASF_CALLBACK_CODECS_FILE}"
            "${callback_codecs_api}"
        VERBATIM)

    add_custom_target(${module_name}Stub
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${published_scripts_directory}/stub"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${stub}"
            "${published_stub}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${metadata}"
            "${published_metadata}"
        DEPENDS "${bindings_stamp}"
        VERBATIM)
    set(${output_variable} "${bindings}" PARENT_SCOPE)
endfunction()
