#include "GpuApi.hpp"

#include <SFML/Window/Context.hpp>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace ludork::runtime::graphics {
namespace {
template <typename T>
T function(const char* name, const char* alternative = nullptr,
           bool required = true) {
    sf::GlFunctionPointer pointer = sf::Context::getFunction(name);
    if (pointer == nullptr && alternative != nullptr) {
        pointer = sf::Context::getFunction(alternative);
    }
    if (pointer == nullptr && required) {
        throw std::runtime_error(
            std::string("GPU Emitter requires OpenGL function ") + name);
    }
    return reinterpret_cast<T>(pointer);
}
}  // namespace

GpuApi::GpuApi() {
#define LOAD(NAME) NAME = function<NAME##Function>("gl" #NAME)
    LOAD(GetString);
    LOAD(GetIntegerv);
    LOAD(GetBooleanv);
    LOAD(GetFloatv);
    LOAD(IsEnabled);
    LOAD(Enable);
    LOAD(Disable);
    LOAD(GenBuffers);
    LOAD(DeleteBuffers);
    LOAD(BindBuffer);
    LOAD(BufferData);
    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(GetShaderInfoLog);
    LOAD(DeleteShader);
    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(BindAttribLocation);
    LOAD(LinkProgram);
    LOAD(GetProgramiv);
    LOAD(GetProgramInfoLog);
    LOAD(DeleteProgram);
    LOAD(UseProgram);
    LOAD(GetUniformLocation);
    LOAD(Uniform1f);
    LOAD(Uniform1i);
    LOAD(Uniform2f);
    LOAD(Uniform4f);
    LOAD(UniformMatrix4fv);
    LOAD(EnableVertexAttribArray);
    LOAD(DisableVertexAttribArray);
    LOAD(VertexAttribPointer);
    LOAD(GetVertexAttribiv);
    LOAD(GetVertexAttribPointerv);
    LOAD(DrawArrays);
    LOAD(ActiveTexture);
    LOAD(GenTextures);
    LOAD(DeleteTextures);
    LOAD(BindTexture);
    LOAD(TexParameteri);
    LOAD(TexImage2D);
    LOAD(Viewport);
    LOAD(BlendFuncSeparate);
    LOAD(BlendEquationSeparate);
    LOAD(ClearColor);
    LOAD(Clear);
    LOAD(ColorMask);
    LOAD(PixelStorei);
    LOAD(ReadPixels);
    LOAD(Flush);
#undef LOAD
    const char* version = reinterpret_cast<const char*>(GetString(0x1F02));
    if (version == nullptr) {
        throw std::runtime_error(
            "GPU Emitter requires an active OpenGL context");
    }
    embedded = std::string(version).find("OpenGL ES") != std::string::npos;
    int major = 0;
    int minor = 0;
    const char* versionNumber = version;
    while (*versionNumber && (*versionNumber < '0' || *versionNumber > '9')) {
        ++versionNumber;
    }
    if (std::sscanf(versionNumber, "%d.%d", &major, &minor) != 2) {
        throw std::runtime_error(
            "Cannot determine the GPU Emitter OpenGL version");
    }
    GetStringi = function<GetStringiFunction>("glGetStringi", nullptr, false);
    std::string extensionText;
    if (major >= 3 && GetStringi != nullptr) {
        int extensionCount = 0;
        GetIntegerv(0x821D, &extensionCount);
        for (int index = 0; index < extensionCount; ++index) {
            const auto* extension = GetStringi(0x1F03, static_cast<E>(index));
            if (extension != nullptr) {
                extensionText += reinterpret_cast<const char*>(extension);
                extensionText += ' ';
            }
        }
    } else {
        const auto* extensions = GetString(0x1F03);
        extensionText = extensions == nullptr
                            ? ""
                            : reinterpret_cast<const char*>(extensions);
        extensionText += ' ';
    }
    const auto hasExtension = [&extensionText](const std::string& name) {
        const std::string token = name + ' ';
        const std::size_t offset = extensionText.find(token);
        return offset != std::string::npos &&
               (offset == 0 || extensionText[offset - 1] == ' ');
    };
    const auto desktopAtLeast = [major, minor, this](int minimumMajor,
                                                     int minimumMinor) {
        return !embedded && (major > minimumMajor ||
                             (major == minimumMajor && minor >= minimumMinor));
    };
    if (embedded && major < 3) {
        throw std::runtime_error("GPU Emitter requires OpenGL ES 3.0 or newer");
    }
    if (!embedded &&
        (!desktopAtLeast(2, 1) ||
         (!desktopAtLeast(3, 0) &&
          !hasExtension("GL_EXT_transform_feedback")) ||
         (!desktopAtLeast(3, 1) && !hasExtension("GL_ARB_draw_instanced")) ||
         (!desktopAtLeast(3, 3) && !hasExtension("GL_ARB_instanced_arrays")) ||
         (!desktopAtLeast(3, 0) && !hasExtension("GL_ARB_texture_float")))) {
        throw std::runtime_error(
            "GPU Emitter requires OpenGL 2.1 with EXT_transform_feedback, "
            "ARB_draw_instanced, ARB_instanced_arrays and ARB_texture_float "
            "or equivalent core support");
    }
    const bool coreFeedback = embedded || desktopAtLeast(3, 0);
    BindBufferBase = function<BindBufferBaseFunction>(
        coreFeedback ? "glBindBufferBase" : "glBindBufferBaseEXT");
    BeginTransformFeedback = function<BeginTransformFeedbackFunction>(
        coreFeedback ? "glBeginTransformFeedback"
                     : "glBeginTransformFeedbackEXT");
    EndTransformFeedback = function<EndTransformFeedbackFunction>(
        coreFeedback ? "glEndTransformFeedback" : "glEndTransformFeedbackEXT");
    TransformFeedbackVaryings = function<TransformFeedbackVaryingsFunction>(
        coreFeedback ? "glTransformFeedbackVaryings"
                     : "glTransformFeedbackVaryingsEXT");
    GetIntegeri_v = function<GetIntegeri_vFunction>(
        coreFeedback ? "glGetIntegeri_v" : "glGetIntegerIndexedvEXT");
    DrawArraysInstanced = function<DrawArraysInstancedFunction>(
        embedded || desktopAtLeast(3, 1) ? "glDrawArraysInstanced"
                                         : "glDrawArraysInstancedARB");
    VertexAttribDivisor = function<VertexAttribDivisorFunction>(
        embedded || desktopAtLeast(3, 3) ? "glVertexAttribDivisor"
                                         : "glVertexAttribDivisorARB");
    renderer = reinterpret_cast<const char*>(GetString(0x1F01));
    int vertexTextures = 0;
    int feedbackComponents = 0;
    int attributes = 0;
    GetIntegerv(0x8B4C, &vertexTextures);
    GetIntegerv(0x8C8A, &feedbackComponents);
    GetIntegerv(0x8869, &attributes);
    if (vertexTextures < 1 || feedbackComponents < 24 || attributes < 7) {
        throw std::runtime_error(
            "GPU Emitter requires vertex texture sampling, 24 "
            "transform-feedback components and 7 vertex attributes");
    }
    const bool coreTimer = !embedded && (desktopAtLeast(3, 3) ||
                                         hasExtension("GL_ARB_timer_query"));
#define OPTIONAL(NAME, ENTRY) \
    NAME = function<NAME##Function>(ENTRY, nullptr, false)
    OPTIONAL(GenQueries, embedded ? "glGenQueriesEXT" : "glGenQueries");
    OPTIONAL(DeleteQueries,
             embedded ? "glDeleteQueriesEXT" : "glDeleteQueries");
    OPTIONAL(BeginQuery, embedded ? "glBeginQueryEXT" : "glBeginQuery");
    OPTIONAL(EndQuery, embedded ? "glEndQueryEXT" : "glEndQuery");
    OPTIONAL(GetQueryObjectiv,
             embedded ? "glGetQueryObjectivEXT" : "glGetQueryObjectiv");
    OPTIONAL(GetQueryObjectui64v,
             coreTimer ? "glGetQueryObjectui64v" : "glGetQueryObjectui64vEXT");
    timerQueries =
        GenQueries && DeleteQueries && BeginQuery && EndQuery &&
        GetQueryObjectiv && GetQueryObjectui64v &&
        (embedded ? hasExtension("GL_EXT_disjoint_timer_query")
                  : (coreTimer || hasExtension("GL_EXT_timer_query")));
    const bool coreFramebuffer = embedded || desktopAtLeast(3, 0) ||
                                 hasExtension("GL_ARB_framebuffer_object");
    OPTIONAL(GenFramebuffers,
             coreFramebuffer ? "glGenFramebuffers" : "glGenFramebuffersEXT");
    OPTIONAL(DeleteFramebuffers, coreFramebuffer ? "glDeleteFramebuffers"
                                                 : "glDeleteFramebuffersEXT");
    OPTIONAL(BindFramebuffer,
             coreFramebuffer ? "glBindFramebuffer" : "glBindFramebufferEXT");
    OPTIONAL(FramebufferTexture2D, coreFramebuffer
                                       ? "glFramebufferTexture2D"
                                       : "glFramebufferTexture2DEXT");
    OPTIONAL(CheckFramebufferStatus, coreFramebuffer
                                         ? "glCheckFramebufferStatus"
                                         : "glCheckFramebufferStatusEXT");
    const bool coreMapping = embedded || desktopAtLeast(3, 0) ||
                             hasExtension("GL_ARB_map_buffer_range");
    OPTIONAL(MapBufferRange,
             coreMapping ? "glMapBufferRange" : "glMapBufferRangeEXT");
    OPTIONAL(UnmapBuffer, "glUnmapBuffer");
    const bool coreSync =
        embedded || desktopAtLeast(3, 2) || hasExtension("GL_ARB_sync");
    OPTIONAL(FenceSync, coreSync ? "glFenceSync" : "glFenceSyncAPPLE");
    OPTIONAL(DeleteSync, coreSync ? "glDeleteSync" : "glDeleteSyncAPPLE");
    OPTIONAL(ClientWaitSync,
             coreSync ? "glClientWaitSync" : "glClientWaitSyncAPPLE");
#undef OPTIONAL
    separateFramebuffers = embedded || desktopAtLeast(3, 0) ||
                           hasExtension("GL_ARB_framebuffer_object") ||
                           hasExtension("GL_EXT_framebuffer_blit");
    framebufferSrgb = embedded ? hasExtension("GL_EXT_sRGB_write_control")
                               : (desktopAtLeast(3, 0) ||
                                  hasExtension("GL_ARB_framebuffer_sRGB") ||
                                  hasExtension("GL_EXT_framebuffer_sRGB"));
    const bool sync = embedded || desktopAtLeast(3, 2) ||
                      hasExtension("GL_ARB_sync") ||
                      hasExtension("GL_APPLE_sync");
    const bool mapping = embedded || desktopAtLeast(3, 0) ||
                         hasExtension("GL_ARB_map_buffer_range") ||
                         hasExtension("GL_EXT_map_buffer_range");
    const bool framebuffer = embedded || desktopAtLeast(3, 0) ||
                             hasExtension("GL_ARB_framebuffer_object") ||
                             hasExtension("GL_EXT_framebuffer_object");
    statisticsAvailable = sync && mapping && framebuffer && GenFramebuffers &&
                          DeleteFramebuffers && BindFramebuffer &&
                          FramebufferTexture2D && CheckFramebufferStatus &&
                          MapBufferRange && UnmapBuffer && FenceSync &&
                          DeleteSync && ClientWaitSync;
    const bool appleArrays = !embedded && !desktopAtLeast(3, 0) &&
                             !hasExtension("GL_ARB_vertex_array_object") &&
                             hasExtension("GL_APPLE_vertex_array_object");
    GenVertexArrays = function<GenVertexArraysFunction>(
        appleArrays ? "glGenVertexArraysAPPLE" : "glGenVertexArrays", nullptr,
        false);
    DeleteVertexArrays = function<DeleteVertexArraysFunction>(
        appleArrays ? "glDeleteVertexArraysAPPLE" : "glDeleteVertexArrays",
        nullptr, false);
    BindVertexArray = function<BindVertexArrayFunction>(
        appleArrays ? "glBindVertexArrayAPPLE" : "glBindVertexArray", nullptr,
        false);
    vertexArraysAvailable =
        (embedded || desktopAtLeast(3, 0) ||
         hasExtension("GL_ARB_vertex_array_object") || appleArrays) &&
        GenVertexArrays && DeleteVertexArrays && BindVertexArray;
    if (embedded && !vertexArraysAvailable) {
        throw std::runtime_error(
            "OpenGL ES 3 GPU Emitter requires vertex array functions");
    }
}

GpuApi::E GpuApi::program(const std::string& vertex,
                          const std::string& fragment, bool feedback) const {
    const E result = CreateProgram();
    std::vector<E> shaders;
    try {
        for (const auto& stage :
             std::array<std::pair<E, const std::string*>, 2>{
                 {{0x8B31, &vertex}, {0x8B30, &fragment}}}) {
            const E shader = CreateShader(stage.first);
            shaders.push_back(shader);
            const char* source = stage.second->c_str();
            ShaderSource(shader, 1, &source, nullptr);
            CompileShader(shader);
            int success = 0;
            GetShaderiv(shader, 0x8B81, &success);
            if (success == 0) {
                char log[8192]{};
                GetShaderInfoLog(shader, sizeof(log), nullptr, log);
                throw std::runtime_error(std::string("GPU Emitter shader: ") +
                                         log);
            }
            AttachShader(result, shader);
        }
        for (unsigned int index = 0; index < 6; ++index) {
            const std::string name = "a" + std::to_string(index);
            BindAttribLocation(result, index, name.c_str());
        }
        BindAttribLocation(result, 6, "corner");
        if (feedback) {
            const char* outputs[] = {"s0", "s1", "s2", "s3", "s4", "s5"};
            TransformFeedbackVaryings(result, 6, outputs, 0x8C8C);
        }
        LinkProgram(result);
        int success = 0;
        GetProgramiv(result, 0x8B82, &success);
        if (success == 0) {
            char log[8192]{};
            GetProgramInfoLog(result, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("GPU Emitter program: ") +
                                     log);
        }
    } catch (...) {
        for (E shader : shaders) {
            DeleteShader(shader);
        }
        DeleteProgram(result);
        throw;
    }
    for (E shader : shaders) {
        DeleteShader(shader);
    }
    return result;
}

GpuStateGuard::GpuStateGuard(GpuApi& api) : api_(api) {
    api_.GetIntegerv(0x8B8D, &program_);
    api_.GetIntegerv(0x8894, &buffer_);
    api_.GetIntegerv(0x8C8F, &feedback_);
    api_.GetIntegeri_v(0x8C8F, 0, &feedbackBase_);
    api_.GetIntegerv(0x84E0, &activeTexture_);
    for (int index = 0; index < 2; ++index) {
        api_.ActiveTexture(0x84C0 + index);
        api_.GetIntegerv(0x8069, &textures_[index]);
    }
    api_.GetIntegerv(0x0BA2, viewport_);
    const unsigned int blendNames[] = {0x80C9, 0x80C8, 0x80CB,
                                       0x80CA, 0x8009, 0x883D};
    for (int index = 0; index < 6; ++index) {
        api_.GetIntegerv(blendNames[index], &blend_[index]);
    }
    blending_ = api_.IsEnabled(0x0BE2) != 0;
    discard_ = api_.IsEnabled(0x8C89) != 0;
    if (api_.vertexArraysAvailable) {
        api_.GetIntegerv(0x85B5, &vertexArray_);
        if (api_.vertexArrayDepth_ == api_.vertexArrays_.size()) {
            api_.vertexArrays_.emplace_back(0);
            api_.GenVertexArrays(1, &api_.vertexArrays_.back());
            if (api_.vertexArrays_.back() == 0) {
                api_.vertexArrays_.pop_back();
                api_.ActiveTexture(activeTexture_);
                throw std::runtime_error(
                    "Failed to create GPU Emitter private vertex array");
            }
        }
        api_.BindVertexArray(api_.vertexArrays_[api_.vertexArrayDepth_++]);
        return;
    }
    for (unsigned int index = 0; index < 7; ++index) {
        Attribute& attribute = attributes_[index];
        api_.GetVertexAttribiv(index, 0x8622, &attribute.enabled);
        api_.GetVertexAttribiv(index, 0x8623, &attribute.size);
        api_.GetVertexAttribiv(index, 0x8625, &attribute.type);
        api_.GetVertexAttribiv(index, 0x886A, &attribute.normalized);
        api_.GetVertexAttribiv(index, 0x8624, &attribute.stride);
        api_.GetVertexAttribiv(index, 0x889F, &attribute.buffer);
        api_.GetVertexAttribiv(index, 0x88FE, &attribute.divisor);
        api_.GetVertexAttribPointerv(index, 0x8645, &attribute.pointer);
    }
}

GpuStateGuard::~GpuStateGuard() {
    if (api_.vertexArraysAvailable) {
        api_.BindVertexArray(static_cast<unsigned int>(vertexArray_));
        --api_.vertexArrayDepth_;
    } else {
        for (unsigned int index = 0; index < 7; ++index) {
            const Attribute& attribute = attributes_[index];
            api_.BindBuffer(0x8892, attribute.buffer);
            api_.VertexAttribPointer(
                index, attribute.size, attribute.type,
                static_cast<unsigned char>(attribute.normalized),
                attribute.stride, attribute.pointer);
            api_.VertexAttribDivisor(index, attribute.divisor);
            if (attribute.enabled) {
                api_.EnableVertexAttribArray(index);
            } else {
                api_.DisableVertexAttribArray(index);
            }
        }
    }
    api_.BindBuffer(0x8892, buffer_);
    api_.BindBufferBase(0x8C8E, 0, feedbackBase_);
    api_.BindBuffer(0x8C8E, feedback_);
    api_.UseProgram(program_);
    for (int index = 0; index < 2; ++index) {
        api_.ActiveTexture(0x84C0 + index);
        api_.BindTexture(0x0DE1, textures_[index]);
    }
    api_.ActiveTexture(activeTexture_);
    api_.Viewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    api_.BlendFuncSeparate(blend_[0], blend_[1], blend_[2], blend_[3]);
    api_.BlendEquationSeparate(blend_[4], blend_[5]);
    if (blending_) {
        api_.Enable(0x0BE2);
    } else {
        api_.Disable(0x0BE2);
    }
    if (discard_) {
        api_.Enable(0x8C89);
    } else {
        api_.Disable(0x8C89);
    }
}
}  // namespace ludork::runtime::graphics
