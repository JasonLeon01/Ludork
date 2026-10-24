#include "GpuGlConstants.hpp"
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
    const char* version =
        reinterpret_cast<const char*>(GetString(gpu_gl::Version));
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
        GetIntegerv(gpu_gl::NumExtensions, &extensionCount);
        for (int index = 0; index < extensionCount; ++index) {
            const auto* extension =
                GetStringi(gpu_gl::Extensions, static_cast<E>(index));
            if (extension != nullptr) {
                extensionText += reinterpret_cast<const char*>(extension);
                extensionText += ' ';
            }
        }
    } else {
        const auto* extensions = GetString(gpu_gl::Extensions);
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
    renderer = reinterpret_cast<const char*>(GetString(gpu_gl::Renderer));
    int vertexTextures = 0;
    int feedbackComponents = 0;
    int attributes = 0;
    GetIntegerv(gpu_gl::MaxVertexTextureImageUnits, &vertexTextures);
    GetIntegerv(gpu_gl::MaxTransformFeedbackInterleavedComponents,
                &feedbackComponents);
    GetIntegerv(gpu_gl::MaxVertexAttribs, &attributes);
    if (vertexTextures < 1 ||
        feedbackComponents <
            static_cast<int>(gpu_particle_layout::StateFloatCount) ||
        attributes <
            static_cast<int>(gpu_particle_layout::VertexAttributeCount)) {
        throw std::runtime_error(
            "GPU Emitter requires vertex texture sampling, " +
            std::to_string(gpu_particle_layout::StateFloatCount) +
            " transform-feedback components and " +
            std::to_string(gpu_particle_layout::VertexAttributeCount) +
            " vertex attributes");
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
                 {{gpu_gl::VertexShader, &vertex},
                  {gpu_gl::FragmentShader, &fragment}}}) {
            const E shader = CreateShader(stage.first);
            shaders.push_back(shader);
            const char* source = stage.second->c_str();
            ShaderSource(shader, 1, &source, nullptr);
            CompileShader(shader);
            int success = 0;
            GetShaderiv(shader, gpu_gl::CompileStatus, &success);
            if (success == 0) {
                char log[ShaderLogCapacity]{};
                GetShaderInfoLog(shader, sizeof(log), nullptr, log);
                throw std::runtime_error(std::string("GPU Emitter shader: ") +
                                         log);
            }
            AttachShader(result, shader);
        }
        for (unsigned int index = 0;
             index < gpu_particle_layout::StateAttributeCount; ++index) {
            BindAttribLocation(result, index,
                               gpu_particle_layout::StateAttributeNames[index]);
        }
        BindAttribLocation(result, gpu_particle_layout::CornerAttributeIndex,
                           gpu_particle_layout::CornerAttributeName);
        if (feedback) {
            TransformFeedbackVaryings(result,
                                      gpu_particle_layout::StateAttributeCount,
                                      gpu_particle_layout::FeedbackNames.data(),
                                      gpu_gl::InterleavedAttribs);
        }
        LinkProgram(result);
        int success = 0;
        GetProgramiv(result, gpu_gl::LinkStatus, &success);
        if (success == 0) {
            char log[ShaderLogCapacity]{};
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
    api_.GetIntegerv(gpu_gl::CurrentProgram, &program_);
    api_.GetIntegerv(gpu_gl::ArrayBufferBinding, &buffer_);
    api_.GetIntegerv(gpu_gl::TransformFeedbackBufferBinding, &feedback_);
    api_.GetIntegeri_v(gpu_gl::TransformFeedbackBufferBinding,
                       gpu_particle_layout::FeedbackBindingIndex,
                       &feedbackBase_);
    api_.GetIntegerv(gpu_gl::ActiveTexture, &activeTexture_);
    for (int index = 0;
         index < static_cast<int>(gpu_particle_layout::TextureUnitCount);
         ++index) {
        api_.ActiveTexture(gpu_gl::Texture0 + index);
        api_.GetIntegerv(gpu_gl::TextureBinding2D, &textures_[index]);
    }
    api_.GetIntegerv(gpu_gl::Viewport, viewport_);
    const unsigned int blendNames[] = {
        gpu_gl::BlendSrcRgb,      gpu_gl::BlendDstRgb,
        gpu_gl::BlendSrcAlpha,    gpu_gl::BlendDstAlpha,
        gpu_gl::BlendEquationRgb, gpu_gl::BlendEquationAlpha};
    for (int index = 0; index < static_cast<int>(std::size(blendNames));
         ++index) {
        api_.GetIntegerv(blendNames[index], &blend_[index]);
    }
    blending_ = api_.IsEnabled(gpu_gl::Blend) != 0;
    discard_ = api_.IsEnabled(gpu_gl::RasterizerDiscard) != 0;
    if (api_.vertexArraysAvailable) {
        api_.GetIntegerv(gpu_gl::VertexArrayBinding, &vertexArray_);
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
    for (unsigned int index = 0;
         index < gpu_particle_layout::VertexAttributeCount; ++index) {
        Attribute& attribute = attributes_[index];
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArrayEnabled,
                               &attribute.enabled);
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArraySize,
                               &attribute.size);
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArrayType,
                               &attribute.type);
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArrayNormalized,
                               &attribute.normalized);
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArrayStride,
                               &attribute.stride);
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArrayBufferBinding,
                               &attribute.buffer);
        api_.GetVertexAttribiv(index, gpu_gl::VertexAttribArrayDivisor,
                               &attribute.divisor);
        api_.GetVertexAttribPointerv(index, gpu_gl::VertexAttribArrayPointer,
                                     &attribute.pointer);
    }
}

GpuStateGuard::~GpuStateGuard() {
    if (api_.vertexArraysAvailable) {
        api_.BindVertexArray(static_cast<unsigned int>(vertexArray_));
        --api_.vertexArrayDepth_;
    } else {
        for (unsigned int index = 0;
             index < gpu_particle_layout::VertexAttributeCount; ++index) {
            const Attribute& attribute = attributes_[index];
            api_.BindBuffer(gpu_gl::ArrayBuffer, attribute.buffer);
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
    api_.BindBuffer(gpu_gl::ArrayBuffer, buffer_);
    api_.BindBufferBase(gpu_gl::TransformFeedbackBuffer,
                        gpu_particle_layout::FeedbackBindingIndex,
                        feedbackBase_);
    api_.BindBuffer(gpu_gl::TransformFeedbackBuffer, feedback_);
    api_.UseProgram(program_);
    for (int index = 0;
         index < static_cast<int>(gpu_particle_layout::TextureUnitCount);
         ++index) {
        api_.ActiveTexture(gpu_gl::Texture0 + index);
        api_.BindTexture(gpu_gl::Texture2D, textures_[index]);
    }
    api_.ActiveTexture(activeTexture_);
    api_.Viewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    api_.BlendFuncSeparate(blend_[0], blend_[1], blend_[2], blend_[3]);
    api_.BlendEquationSeparate(blend_[4], blend_[5]);
    if (blending_) {
        api_.Enable(gpu_gl::Blend);
    } else {
        api_.Disable(gpu_gl::Blend);
    }
    if (discard_) {
        api_.Enable(gpu_gl::RasterizerDiscard);
    } else {
        api_.Disable(gpu_gl::RasterizerDiscard);
    }
}
}  // namespace ludork::runtime::graphics
