#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32)
#define EMITTER_GL_CALL __stdcall
#else
#define EMITTER_GL_CALL
#endif

namespace ludork::runtime::graphics {

class GpuApi {
public:
    using E = unsigned int;
    using I = int;
    using S = std::ptrdiff_t;
    using B = unsigned char;
#define EMITTER_GL_FUNCTION(RET, NAME, ...)                    \
    using NAME##Function = RET(EMITTER_GL_CALL*)(__VA_ARGS__); \
    NAME##Function NAME = nullptr;
    EMITTER_GL_FUNCTION(const unsigned char*, GetString, E)
    EMITTER_GL_FUNCTION(const unsigned char*, GetStringi, E, E)
    EMITTER_GL_FUNCTION(void, GetIntegerv, E, I*)
    EMITTER_GL_FUNCTION(void, GetBooleanv, E, B*)
    EMITTER_GL_FUNCTION(void, GetFloatv, E, float*)
    EMITTER_GL_FUNCTION(B, IsEnabled, E)
    EMITTER_GL_FUNCTION(void, Enable, E)
    EMITTER_GL_FUNCTION(void, Disable, E)
    EMITTER_GL_FUNCTION(void, GenBuffers, I, E*)
    EMITTER_GL_FUNCTION(void, DeleteBuffers, I, const E*)
    EMITTER_GL_FUNCTION(void, BindBuffer, E, E)
    EMITTER_GL_FUNCTION(void, BufferData, E, S, const void*, E)
    EMITTER_GL_FUNCTION(void, BindBufferBase, E, E, E)
    EMITTER_GL_FUNCTION(void, BeginTransformFeedback, E)
    EMITTER_GL_FUNCTION(void, EndTransformFeedback)
    EMITTER_GL_FUNCTION(void, TransformFeedbackVaryings, E, I,
                        const char* const*, E)
    EMITTER_GL_FUNCTION(E, CreateShader, E)
    EMITTER_GL_FUNCTION(void, ShaderSource, E, I, const char* const*, const I*)
    EMITTER_GL_FUNCTION(void, CompileShader, E)
    EMITTER_GL_FUNCTION(void, GetShaderiv, E, E, I*)
    EMITTER_GL_FUNCTION(void, GetShaderInfoLog, E, I, I*, char*)
    EMITTER_GL_FUNCTION(void, DeleteShader, E)
    EMITTER_GL_FUNCTION(E, CreateProgram)
    EMITTER_GL_FUNCTION(void, AttachShader, E, E)
    EMITTER_GL_FUNCTION(void, BindAttribLocation, E, E, const char*)
    EMITTER_GL_FUNCTION(void, LinkProgram, E)
    EMITTER_GL_FUNCTION(void, GetProgramiv, E, E, I*)
    EMITTER_GL_FUNCTION(void, GetProgramInfoLog, E, I, I*, char*)
    EMITTER_GL_FUNCTION(void, DeleteProgram, E)
    EMITTER_GL_FUNCTION(void, UseProgram, E)
    EMITTER_GL_FUNCTION(I, GetUniformLocation, E, const char*)
    EMITTER_GL_FUNCTION(void, Uniform1f, I, float)
    EMITTER_GL_FUNCTION(void, Uniform1i, I, I)
    EMITTER_GL_FUNCTION(void, Uniform2f, I, float, float)
    EMITTER_GL_FUNCTION(void, Uniform4f, I, float, float, float, float)
    EMITTER_GL_FUNCTION(void, UniformMatrix4fv, I, I, B, const float*)
    EMITTER_GL_FUNCTION(void, EnableVertexAttribArray, E)
    EMITTER_GL_FUNCTION(void, DisableVertexAttribArray, E)
    EMITTER_GL_FUNCTION(void, VertexAttribPointer, E, I, E, B, I, const void*)
    EMITTER_GL_FUNCTION(void, VertexAttribDivisor, E, E)
    EMITTER_GL_FUNCTION(void, GetVertexAttribiv, E, E, I*)
    EMITTER_GL_FUNCTION(void, GetVertexAttribPointerv, E, E, void**)
    EMITTER_GL_FUNCTION(void, DrawArrays, E, I, I)
    EMITTER_GL_FUNCTION(void, DrawArraysInstanced, E, I, I, I)
    EMITTER_GL_FUNCTION(void, ActiveTexture, E)
    EMITTER_GL_FUNCTION(void, GenTextures, I, E*)
    EMITTER_GL_FUNCTION(void, DeleteTextures, I, const E*)
    EMITTER_GL_FUNCTION(void, BindTexture, E, E)
    EMITTER_GL_FUNCTION(void, TexParameteri, E, E, I)
    EMITTER_GL_FUNCTION(void, TexImage2D, E, I, I, I, I, I, E, E, const void*)
    EMITTER_GL_FUNCTION(void, Viewport, I, I, I, I)
    EMITTER_GL_FUNCTION(void, BlendFuncSeparate, E, E, E, E)
    EMITTER_GL_FUNCTION(void, BlendEquationSeparate, E, E)
    EMITTER_GL_FUNCTION(void, GetIntegeri_v, E, E, I*)
    EMITTER_GL_FUNCTION(void, GenQueries, I, E*)
    EMITTER_GL_FUNCTION(void, DeleteQueries, I, const E*)
    EMITTER_GL_FUNCTION(void, BeginQuery, E, E)
    EMITTER_GL_FUNCTION(void, EndQuery, E)
    EMITTER_GL_FUNCTION(void, GetQueryObjectiv, E, E, I*)
    EMITTER_GL_FUNCTION(void, GetQueryObjectui64v, E, E, std::uint64_t*)
    EMITTER_GL_FUNCTION(void, GenFramebuffers, I, E*)
    EMITTER_GL_FUNCTION(void, DeleteFramebuffers, I, const E*)
    EMITTER_GL_FUNCTION(void, BindFramebuffer, E, E)
    EMITTER_GL_FUNCTION(void, FramebufferTexture2D, E, E, E, E, I)
    EMITTER_GL_FUNCTION(E, CheckFramebufferStatus, E)
    EMITTER_GL_FUNCTION(void, ClearColor, float, float, float, float)
    EMITTER_GL_FUNCTION(void, Clear, E)
    EMITTER_GL_FUNCTION(void, ColorMask, B, B, B, B)
    EMITTER_GL_FUNCTION(void, PixelStorei, E, I)
    EMITTER_GL_FUNCTION(void, ReadPixels, I, I, I, I, E, E, void*)
    EMITTER_GL_FUNCTION(void*, MapBufferRange, E, S, S, E)
    EMITTER_GL_FUNCTION(B, UnmapBuffer, E)
    EMITTER_GL_FUNCTION(void*, FenceSync, E, E)
    EMITTER_GL_FUNCTION(void, DeleteSync, void*)
    EMITTER_GL_FUNCTION(E, ClientWaitSync, void*, E, std::uint64_t)
    EMITTER_GL_FUNCTION(void, Flush)
    EMITTER_GL_FUNCTION(void, GenVertexArrays, I, E*)
    EMITTER_GL_FUNCTION(void, DeleteVertexArrays, I, const E*)
    EMITTER_GL_FUNCTION(void, BindVertexArray, E)
#undef EMITTER_GL_FUNCTION
    bool embedded = false;
    bool timerQueries = false;
    bool statisticsAvailable = false;
    bool separateFramebuffers = false;
    bool framebufferSrgb = false;
    bool vertexArraysAvailable = false;
    std::string renderer;
    GpuApi();
    E program(const std::string& vertex, const std::string& fragment,
              bool feedback) const;

private:
    friend class GpuResourcesImpl;
    friend class GpuStateGuard;
    std::vector<E> vertexArrays_;
    std::size_t vertexArrayDepth_ = 0;
};

class GpuStateGuard {
public:
    explicit GpuStateGuard(GpuApi& api);
    ~GpuStateGuard();

private:
    struct Attribute {
        int enabled, size, type, normalized, stride, buffer, divisor;
        void* pointer;
    };
    GpuApi& api_;
    int program_, buffer_, feedback_, feedbackBase_, activeTexture_,
        textures_[2];
    int viewport_[4], blend_[6];
    bool blending_, discard_;
    int vertexArray_ = 0;
    Attribute attributes_[7];
};

}  // namespace ludork::runtime::graphics

#undef EMITTER_GL_CALL
