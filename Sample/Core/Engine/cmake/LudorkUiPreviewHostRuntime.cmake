include_guard(GLOBAL)

function(ludork_add_ui_preview_host_runtime target)
    file(GLOB_RECURSE preview_runtime_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Input/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/*.cpp")
    list(APPEND preview_runtime_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Animation.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Curve.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineState.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Gameplay/Actor/VisualRuntime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/RectBase.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/File.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/Math.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/Render.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/ShaderLoader.cpp")
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND preview_runtime_sources
            "${CMAKE_CURRENT_SOURCE_DIR}/src/Input/InputService/Platform/PlatformInputBridgeMac.mm")
    endif()

    add_library(${target} SHARED ${preview_runtime_sources})
    add_library(Ludork::UiPreviewHostRuntime ALIAS ${target})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE
        LUDORK_ENGINE_EXPORTS=1
        LUDORK_UI_PREVIEW_HOST_RUNTIME=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}")
    target_include_directories(${target}
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            ${LUDORK_LUASF_CONSUMER_INCLUDES}
        PRIVATE
            "${LUDORK_LUASF_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/UI")
    target_link_libraries(${target}
        PUBLIC
            Ludork::Runtime
            Ludork::Standard
            SFML::Graphics
        PRIVATE
            LuaSF::Lua
            zlibstatic)
    if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
        target_link_libraries(${target} PRIVATE deviceinfo_ndk.z)
    endif()
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
        find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
        target_link_libraries(${target} PRIVATE
            "${APPKIT_FRAMEWORK}"
            "${COREGRAPHICS_FRAMEWORK}")
    endif()
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        INTERPROCEDURAL_OPTIMIZATION_DEBUG ${LUDORK_CORE_DEBUG_LTO}
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ${LUDORK_CORE_RELEASE_LTO}
        WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    ludork_set_runtime_output(${target})
    ludork_enable_release_symbols(${target})
endfunction()
