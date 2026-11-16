include_guard(GLOBAL)
include(FetchContent)

function(ludork_add_shader_tools)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPIRV ON CACHE BOOL "" FORCE)
    set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(ENABLE_RTTI OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(ludork_glslang
        URL "https://codeload.github.com/KhronosGroup/glslang/tar.gz/b5782e52ee2f7b3e40bb9c80d15b47016e008bc9"
        URL_HASH SHA256=6c242598c332050bf3a3619d299594ca2909426935f2c86c69c7356b049347c7
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(ludork_spirv_cross
        URL "https://codeload.github.com/KhronosGroup/SPIRV-Cross/tar.gz/7affe74d77f93a622bb5002789d5332d32e512ee"
        URL_HASH SHA256=3d54721c7db5cf955c6b5f01be2702ea1154c86d5ef322685a3738788a093b3e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(ludork_glslang ludork_spirv_cross)
endfunction()

ludork_add_shader_tools()

function(ludork_shader_tools_fingerprint translator_output compiler_output)
    get_filename_component(shader_runtime_directory
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../Runtime" ABSOLUTE)
    set(shader_inputs "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/LudorkShaderTools.cmake")
    file(GLOB shader_sources CONFIGURE_DEPENDS
        "${shader_runtime_directory}/src/Graphics/Shaders/*.cpp"
        "${shader_runtime_directory}/src/Graphics/Shaders/*.hpp"
        "${shader_runtime_directory}/include/Runtime/Graphics/ShaderTranslator.hpp")
    list(FILTER shader_sources EXCLUDE REGEX "/ShaderCompiler\\.(cpp|hpp)$")
    list(APPEND shader_inputs ${shader_sources})
    set(identity "")
    foreach(shader_input IN LISTS shader_inputs)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${shader_input}")
        file(SHA256 "${shader_input}" digest)
        string(APPEND identity "${digest}")
    endforeach()
    string(SHA256 identity "${identity}")
    set(${translator_output} "${identity}" PARENT_SCOPE)
    set(compiler_inputs
        "${shader_runtime_directory}/src/Graphics/Shaders/ShaderCompiler.cpp"
        "${shader_runtime_directory}/include/Runtime/Graphics/ShaderCompiler.hpp")
    set(identity "")
    foreach(compiler_input IN LISTS compiler_inputs)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${compiler_input}")
        file(SHA256 "${compiler_input}" digest)
        string(APPEND identity "${digest}")
    endforeach()
    string(SHA256 identity "${identity}")
    set(${compiler_output} "${identity}" PARENT_SCOPE)
endfunction()
