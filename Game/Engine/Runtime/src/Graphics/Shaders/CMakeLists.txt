include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/LudorkShaderTools.cmake")
ludork_shader_tools_fingerprint(shader_tools_fingerprint shader_compiler_fingerprint)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ShaderTranslator.cpp"
    TARGET_DIRECTORY LudorkRuntime APPEND PROPERTY COMPILE_DEFINITIONS
    LUDORK_SHADER_TRANSLATOR_FINGERPRINT="${shader_tools_fingerprint}")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ShaderCompiler.cpp"
    TARGET_DIRECTORY LudorkRuntime APPEND PROPERTY COMPILE_DEFINITIONS
    LUDORK_SHADER_COMPILER_FINGERPRINT="${shader_compiler_fingerprint}")

target_sources(LudorkRuntime PRIVATE ShaderTranslator.cpp ShaderCompiler.cpp
    ShaderSourceTransform.cpp ShaderHlslInterface.cpp)
target_link_libraries(LudorkRuntime PRIVATE
    glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits
    spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
if(WIN32)
    target_link_libraries(LudorkRuntime PRIVATE d3dcompiler version)
endif()
