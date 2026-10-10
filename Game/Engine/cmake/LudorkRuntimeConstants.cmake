include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/LudorkScriptTools.cmake")

set(LUDORK_SCRIPT_TOOLS_EXECUTABLE "" CACHE FILEPATH
    "Path to the platform ScriptTools executable")
if(NOT LUDORK_SCRIPT_TOOLS_EXECUTABLE OR
   NOT EXISTS "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}")
    message(FATAL_ERROR
        "LUDORK_SCRIPT_TOOLS_EXECUTABLE is missing. Run tools/init first.")
endif()

set(LUDORK_RUNTIME_CONSTANTS_DIRECTORY
    "${LUDORK_INTERMEDIATE_DIRECTORY}/RuntimeConstants")
set(LUDORK_SERVER_CONFIG_DIRECTORY
    "${LUDORK_INTERMEDIATE_DIRECTORY}/ServerConfig/$<CONFIG>")
ludork_get_script_tools_dependencies(runtime_constants_dependencies)
add_custom_target(RuntimeConstantsGenerate
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}" runtime-constants cpp
        "${LUDORK_PROJECT_SOURCE_DIR}" "${LUDORK_RUNTIME_CONSTANTS_DIRECTORY}"
    COMMAND "${LUDORK_SCRIPT_TOOLS_EXECUTABLE}" server-config generate
        "${LUDORK_SERVER_CONFIG_DIRECTORY}" "$<CONFIG>"
    BYPRODUCTS
        "${LUDORK_RUNTIME_CONSTANTS_DIRECTORY}/LudorkGenerated/LdPakFormatConstants.hpp"
        "${LUDORK_RUNTIME_CONSTANTS_DIRECTORY}/LudorkGenerated/EncryptedPayloadConstants.hpp"
        "${LUDORK_RUNTIME_CONSTANTS_DIRECTORY}/LudorkGenerated/ResourceFileConstants.hpp"
        "${LUDORK_RUNTIME_CONSTANTS_DIRECTORY}/LudorkGenerated/LightingConstants.hpp"
        "${LUDORK_SERVER_CONFIG_DIRECTORY}/LudorkGenerated/ServerConfig.hpp"
    DEPENDS ${runtime_constants_dependencies}
        "${LUDORK_PROJECT_SOURCE_DIR}/Assets/Shaders/Global/UnoccludedLightPass.frag"
    VERBATIM)
add_library(LudorkRuntimeConstants INTERFACE)
add_library(Ludork::RuntimeConstants ALIAS LudorkRuntimeConstants)
add_dependencies(LudorkRuntimeConstants RuntimeConstantsGenerate)
target_include_directories(LudorkRuntimeConstants INTERFACE
    "${LUDORK_RUNTIME_CONSTANTS_DIRECTORY}" "${LUDORK_SERVER_CONFIG_DIRECTORY}")
