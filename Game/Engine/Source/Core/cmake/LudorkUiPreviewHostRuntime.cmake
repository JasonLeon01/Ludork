include_guard(GLOBAL)

function(ludork_add_ui_preview_host_runtime target)
    file(GLOB_RECURSE preview_runtime_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Input/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Particles/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/*.cpp")
    list(APPEND preview_runtime_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/AnimSprite.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Emitters/Emitter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Emitters/EmitterResource.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Emitters/EmitterCurves.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Emitters/EmitterConfigurationCompiler.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/EmitterScheduler.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Curve.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Curve/CurveMath.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Vector2Curve.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Vector3Curve.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Vector4Curve.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineState.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/RectBase.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/Math.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/Render.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/ShaderLoader.cpp")

    add_library(${target} SHARED ${preview_runtime_sources})
    add_library(Ludork::UiPreviewHostRuntime ALIAS ${target})
    target_compile_definitions(${target} PRIVATE
        LUDORK_ENGINE_EXPORTS=1
        LUDORK_UI_PREVIEW_HOST_RUNTIME=1
        LUDORK_PLATFORM="${LUDORK_PLATFORM}")
    target_include_directories(${target}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    ludork_add_core_shared(${target})
    target_link_libraries(${target}
        PUBLIC
            Ludork::Runtime
            Ludork::Standard
            SFML::Graphics
        PRIVATE zlibstatic Ludork::RuntimeConstants)
    ludork_link_engine_platform(${target})
    ludork_configure_core_target(${target})
    ludork_enable_release_symbols(${target})
endfunction()
