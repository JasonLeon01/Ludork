include_guard(GLOBAL)

function(ludork_add_ios_bundle_directory_sync
    target source_directory bundle_subdirectory)
    set(multi_value_args EXCLUDES)
    cmake_parse_arguments(BUNDLE_SYNC
        ""
        ""
        "${multi_value_args}"
        ${ARGN})

    set(exclude_arguments --exclude=.DS_Store)
    foreach(exclude_pattern IN LISTS BUNDLE_SYNC_EXCLUDES)
        list(APPEND exclude_arguments "--exclude=${exclude_pattern}")
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_BUNDLE_DIR:${target}>/${bundle_subdirectory}"
        COMMAND "${LUDORK_RSYNC_EXECUTABLE}"
            -a
            --delete
            ${exclude_arguments}
            "${source_directory}/"
            "$<TARGET_BUNDLE_DIR:${target}>/${bundle_subdirectory}/"
        VERBATIM)
endfunction()

function(ludork_configure_ios_application target)
    foreach(required_variable IN ITEMS
        LUDORK_IOS_APP_NAME
        LUDORK_IOS_BUNDLE_IDENTIFIER
        LUDORK_IOS_DEVELOPMENT_TEAM
        LUDORK_IOS_INFO_PLIST
        LUDORK_IOS_ICON)
        if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
            message(FATAL_ERROR "${required_variable} is required for an iOS build.")
        endif()
    endforeach()
    if(NOT EXISTS "${LUDORK_IOS_INFO_PLIST}")
        message(FATAL_ERROR "iOS Info.plist was not found: ${LUDORK_IOS_INFO_PLIST}")
    endif()
    if(NOT EXISTS "${LUDORK_IOS_ICON}")
        message(FATAL_ERROR "iOS app icon was not found: ${LUDORK_IOS_ICON}")
    endif()
    target_sources(${target} PRIVATE "${LUDORK_IOS_ICON}")
    set_source_files_properties(
        "${LUDORK_IOS_ICON}"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "${LUDORK_IOS_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_NAME "${LUDORK_IOS_APP_NAME}"
        MACOSX_BUNDLE_INFO_PLIST "${LUDORK_IOS_INFO_PLIST}"
        OUTPUT_NAME "${LUDORK_IOS_APP_NAME}"
        XCODE_GENERATE_SCHEME ON
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${LUDORK_IOS_DEVELOPMENT_TEAM}"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.3"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${LUDORK_IOS_BUNDLE_IDENTIFIER}"
        XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS iphoneos
        "XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release]" YES
        "XCODE_ATTRIBUTE_STRIP_INSTALLED_PRODUCT[variant=Release]" YES
        "XCODE_ATTRIBUTE_STRIP_STYLE[variant=Release]" non-global
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1")
    target_link_libraries(${target} PRIVATE
        SFML::Main)
    find_program(LUDORK_RSYNC_EXECUTABLE rsync REQUIRED)
    set(ludork_resource_layout "")
    foreach(resource_directory IN ITEMS Assets Data Scripts)
        string(TOUPPER "${resource_directory}" resource_directory_upper)
        set(resource_source_variable
            "LUDORK_${resource_directory_upper}_SOURCE_DIR")
        set(resource_package_variable
            "LUDORK_${resource_directory_upper}_PACKAGE_FILE")
        if(NOT DEFINED ${resource_source_variable})
            set(${resource_source_variable}
                "${CMAKE_CURRENT_SOURCE_DIR}/${resource_directory}")
        endif()
        set(${resource_package_variable} "" CACHE FILEPATH
            "Packaged ${resource_directory}.ldpak resource used by the iOS application")
        set(ludork_has_loose_resource OFF)
        if(IS_DIRECTORY "${${resource_source_variable}}")
            set(ludork_has_loose_resource ON)
        endif()
        set(ludork_has_packed_resource OFF)
        if(NOT "${${resource_package_variable}}" STREQUAL "")
            if(NOT EXISTS "${${resource_package_variable}}"
               OR IS_DIRECTORY "${${resource_package_variable}}")
                message(FATAL_ERROR
                    "The iOS resource package is not a file: ${${resource_package_variable}}")
            endif()
            get_filename_component(ludork_resource_package_name
                "${${resource_package_variable}}" NAME)
            if(NOT ludork_resource_package_name STREQUAL "${resource_directory}.ldpak")
                message(FATAL_ERROR
                    "The iOS resource package must be named ${resource_directory}.ldpak: ${${resource_package_variable}}")
            endif()
            set(ludork_has_packed_resource ON)
        endif()
        if((ludork_has_loose_resource AND ludork_has_packed_resource)
           OR (NOT ludork_has_loose_resource AND NOT ludork_has_packed_resource))
            message(FATAL_ERROR
                "The iOS application requires exactly one loose ${resource_directory} directory or ${resource_directory}.ldpak file.")
        endif()
        if(ludork_has_loose_resource)
            set(ludork_current_resource_layout Loose)
        else()
            set(ludork_current_resource_layout Packed)
        endif()
        if(NOT ludork_resource_layout STREQUAL ""
           AND NOT ludork_resource_layout STREQUAL ludork_current_resource_layout)
            message(FATAL_ERROR
                "The iOS Assets, Data, and Scripts resources must use the same loose or packed layout.")
        endif()
        set(ludork_resource_layout "${ludork_current_resource_layout}")
        if(ludork_has_loose_resource)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E rm -f
                    "$<TARGET_BUNDLE_DIR:${target}>/${resource_directory}.ldpak"
                VERBATIM)
            set(resource_excludes "")
            if(NOT resource_directory STREQUAL "Scripts")
                set(resource_excludes "*.anim.json")
            endif()
            ludork_add_ios_bundle_directory_sync(
                ${target}
                "${${resource_source_variable}}"
                "${resource_directory}"
                EXCLUDES ${resource_excludes})
        else()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E rm -rf
                    "$<TARGET_BUNDLE_DIR:${target}>/${resource_directory}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${${resource_package_variable}}"
                    "$<TARGET_BUNDLE_DIR:${target}>/${resource_directory}.ldpak"
                VERBATIM)
        endif()
    endforeach()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Licenses")
        ludork_add_ios_bundle_directory_sync(
            ${target}
            "${CMAKE_CURRENT_SOURCE_DIR}/Licenses"
            Licenses)
    endif()
    foreach(legal_file IN ITEMS
        LICENSE.md
        THIRD_PARTY_NOTICES.md
        THIRD_PARTY_NOTICES_zh_CN.md)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${legal_file}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/${legal_file}"
                    "$<TARGET_BUNDLE_DIR:${target}>/${legal_file}"
                VERBATIM)
        endif()
    endforeach()
    find_program(LUDORK_XATTR_EXECUTABLE xattr REQUIRED)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${LUDORK_XATTR_EXECUTABLE}"
            -cr
            "$<TARGET_BUNDLE_DIR:${target}>"
        VERBATIM)
endfunction()
