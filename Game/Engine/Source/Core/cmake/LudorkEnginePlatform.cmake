include_guard(GLOBAL)

function(ludork_link_engine_platform target)
    if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
        target_link_libraries(${target} PRIVATE deviceinfo_ndk.z)
    endif()
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        target_sources(${target} PRIVATE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/Input/InputService/Platform/PlatformInputBridgeMac.mm")
        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
        find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
        target_link_libraries(${target} PRIVATE
            "${APPKIT_FRAMEWORK}"
            "${COREGRAPHICS_FRAMEWORK}")
    endif()
endfunction()
