include_guard(GLOBAL)

function(ludork_add_ixwebsocket)
    set(ixwebsocket_source_directory
        "${LUDORK_PROJECT_SOURCE_DIR}/Engine/ThirdParty/IXWebSocket")
    if(NOT EXISTS "${ixwebsocket_source_directory}/CMakeLists.txt")
        message(FATAL_ERROR
            "IXWebSocket source was not found: ${ixwebsocket_source_directory}. Run tools/init_cpp_dependencies for this project.")
    endif()

    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_DEMO OFF)
    set(USE_TLS OFF)
    set(USE_NETWORK_FRAMEWORK OFF)
    set(USE_ZLIB OFF)
    set(USE_WS OFF)
    set(USE_TEST OFF)
    set(IXWEBSOCKET_INSTALL OFF)
    add_subdirectory("${ixwebsocket_source_directory}" IXWebSocket EXCLUDE_FROM_ALL)
endfunction()

ludork_add_ixwebsocket()
