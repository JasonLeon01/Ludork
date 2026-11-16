set(LUDORK_WEB_RESOURCE_DIRECTORY "" CACHE PATH "Prepared Web game resources")
set(LUDORK_WEB_APP_NAME "" CACHE STRING "Web game application identity")
set(LUDORK_WEB_TITLE "Ludork" CACHE STRING "Web game display title")
if(NOT IS_DIRECTORY "${LUDORK_WEB_RESOURCE_DIRECTORY}" OR LUDORK_WEB_APP_NAME STREQUAL "")
    message(FATAL_ERROR "Web builds require prepared resources and LUDORK_WEB_APP_NAME; use ludork-pack web-pack.")
endif()

set(ludork_web_host "${LUDORK_PROJECT_SOURCE_DIR}/Engine/PlatformHosts/Web")
string(HEX "${LUDORK_WEB_APP_NAME}" ludork_web_app_name_hex)
string(REGEX REPLACE "(..)" "\\\\x\\1" LUDORK_WEB_APP_NAME_BYTES "${ludork_web_app_name_hex}")
string(HEX "${LUDORK_WEB_TITLE}" ludork_web_title_hex)
string(REGEX REPLACE "(..)" "\\\\x\\1" LUDORK_WEB_TITLE_BYTES "${ludork_web_title_hex}")
configure_file("${ludork_web_host}/WebConfiguration.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/web/WebConfiguration.hpp" @ONLY)
target_include_directories(${LUDORK_APPLICATION_TARGET} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/web")
target_sources(${LUDORK_APPLICATION_TARGET} PRIVATE
    "${LUDORK_APPLICATION_ROOT}/src/Platform/Web/WebApplication.cpp")
set_target_properties(${LUDORK_APPLICATION_TARGET} PROPERTIES SUFFIX ".js")
# Shader translation is synchronous and never invokes game callbacks.
# Keep its large compiler functions outside Asyncify; preparation yields between programs.
target_link_options(${LUDORK_APPLICATION_TARGET} PRIVATE
    --emit-symbol-map
    -lidbfs.js -sFORCE_FILESYSTEM=1 -sEXIT_RUNTIME=1 -sSTACK_SIZE=1048576
    -sASYNCIFY_STACK_SIZE=65536
    "-sASYNCIFY_REMOVE=[\"*ShaderTranslator::translate*\",\"*glslang*\",\"*spirv_cross*\"]"
    "SHELL:--preload-file \"${LUDORK_WEB_RESOURCE_DIRECTORY}@/Game\"")
file(GLOB_RECURSE ludork_web_resources CONFIGURE_DEPENDS "${LUDORK_WEB_RESOURCE_DIRECTORY}/*")
set_property(TARGET ${LUDORK_APPLICATION_TARGET} APPEND PROPERTY LINK_DEPENDS ${ludork_web_resources})
set(ludork_web_output "${LUDORK_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
add_custom_command(OUTPUT "${ludork_web_output}/index.html"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ludork_web_output}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${ludork_web_host}/index.html" "${ludork_web_output}/index.html"
    DEPENDS "${ludork_web_host}/index.html"
    VERBATIM)
add_custom_target(LudorkWebHost DEPENDS "${ludork_web_output}/index.html")
add_dependencies(${LUDORK_APPLICATION_TARGET} LudorkWebHost)
