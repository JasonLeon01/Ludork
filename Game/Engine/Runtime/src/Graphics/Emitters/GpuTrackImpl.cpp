#include "GpuParticleLayout.hpp"
#include "GpuParticleRandom.hpp"
#include "GpuGlConstants.hpp"
#include "GpuTrackImpl.hpp"
#include <SFML/Graphics/Image.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/ConcurrentResourceCache.hpp>
#include <Runtime/Graphics/GpuEmitterCurveLayout.hpp>
#include <Runtime/Graphics/SceneLighting.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ludork::runtime::graphics {
namespace {
std::shared_ptr<sf::Texture> particleTexture(const std::string& path) {
    static ludork::runtime::ConcurrentResourceCache<sf::Texture> cache;
    return cache.getOrLoad(path, [&path]() {
        if (path.empty()) {
            return std::make_shared<sf::Texture>(
                sf::Image({1, 1}, sf::Color::Transparent));
        }
        auto stream = ludork::runtime::assetStore().open(path);
        auto texture = std::make_shared<sf::Texture>();
        if (!texture->loadFromStream(*stream)) {
            throw std::runtime_error("Cannot load particle texture: " + path);
        }
        return texture;
    });
}
sf::Transform withoutScale(const sf::Transform& source) {
    const float* m = source.getMatrix();
    const float x = std::hypot(m[0], m[1]);
    const float y = std::hypot(m[4], m[5]);
    return sf::Transform(x > 0 ? m[0] / x : 1, y > 0 ? m[4] / y : 0, m[12],
                         x > 0 ? m[1] / x : 0, y > 0 ? m[5] / y : 1, m[13], 0,
                         0, 1);
}
}  // namespace

GpuTrackImpl::GpuTrackImpl(const GpuEmitterConfiguration::Track& data,
                           std::shared_ptr<GpuResourcesImpl> gpu)
    : definition(data),
      resources(std::move(gpu)),
      texture(particleTexture(data.texture)) {
    for (unsigned char character : definition.name) {
        seedSalt_ = (seedSalt_ ^ character) * NameHashPrime;
    }
    seedSalt_ %= gpu_particle_random::SeedModulus;
    GpuApi& gl = resources->api;
    GpuStateGuard guard(gl);
    try {
        resources->reserveIndices(definition.capacity);
        gl.GenBuffers(2, buffers);
        for (unsigned int buffer : buffers) {
            gl.BindBuffer(gpu_gl::ArrayBuffer, buffer);
            gl.BufferData(gpu_gl::ArrayBuffer,
                          static_cast<GpuApi::S>(definition.capacity) *
                              gpu_particle_layout::StateStrideBytes,
                          nullptr, gpu_gl::DynamicCopy);
        }
        gl.GenTextures(1, &curves);
        gl.ActiveTexture(gpu_gl::Texture0 +
                         gpu_particle_layout::CurveTextureUnit);
        gl.BindTexture(gpu_gl::Texture2D, curves);
        gl.TexParameteri(gpu_gl::Texture2D, gpu_gl::TextureMinFilter,
                         gpu_gl::Nearest);
        gl.TexParameteri(gpu_gl::Texture2D, gpu_gl::TextureMagFilter,
                         gpu_gl::Nearest);
        gl.TexParameteri(gpu_gl::Texture2D, gpu_gl::TextureWrapS,
                         gpu_gl::ClampToEdge);
        gl.TexParameteri(gpu_gl::Texture2D, gpu_gl::TextureWrapT,
                         gpu_gl::ClampToEdge);
        gl.TexImage2D(gpu_gl::Texture2D, 0, gpu_gl::Rgba32f,
                      static_cast<int>(emitter_curve_layout::SampleCount),
                      static_cast<int>(emitter_curve_layout::TextureRowCount),
                      0, gpu_gl::Rgba, gpu_gl::Float,
                      definition.curveSamples.data());
        const sf::Vector2u size = texture->getSize();
        if (definition.textureRect.size.x == 0 &&
            definition.textureRect.size.y == 0) {
            definition.textureRect = {
                {0, 0}, {static_cast<int>(size.x), static_cast<int>(size.y)}};
        }
        if (definition.textureRect.size.x <= 0 ||
            definition.textureRect.size.y <= 0 ||
            static_cast<std::int64_t>(definition.textureRect.position.x) +
                    definition.textureRect.size.x >
                static_cast<int>(size.x) ||
            static_cast<std::int64_t>(definition.textureRect.position.y) +
                    definition.textureRect.size.y >
                static_cast<int>(size.y)) {
            throw std::invalid_argument(
                "Particle textureRect exceeds texture: " + data.name);
        }
        reset();
    } catch (...) {
        gl.DeleteBuffers(2, buffers);
        gl.DeleteTextures(1, &curves);
        throw;
    }
}
GpuTrackImpl::~GpuTrackImpl() {
    resources->api.DeleteBuffers(2, buffers);
    resources->api.DeleteTextures(1, &curves);
}
void GpuTrackImpl::bindState(bool instanced) {
    GpuApi& gl = resources->api;
    gl.BindBuffer(gpu_gl::ArrayBuffer, buffers[input]);
    for (unsigned int index = 0;
         index < gpu_particle_layout::StateAttributeCount; ++index) {
        gl.EnableVertexAttribArray(index);
        gl.VertexAttribPointer(
            index, gpu_particle_layout::ComponentsPerAttribute, gpu_gl::Float,
            0, gpu_particle_layout::StateStrideBytes,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                index * gpu_particle_layout::ComponentsPerAttribute *
                sizeof(float))));
        gl.VertexAttribDivisor(index, instanced ? 1 : 0);
    }
    gl.EnableVertexAttribArray(gpu_particle_layout::CornerAttributeIndex);
    gl.BindBuffer(gpu_gl::ArrayBuffer,
                  instanced ? resources->corners : resources->indices);
    gl.VertexAttribPointer(
        gpu_particle_layout::CornerAttributeIndex,
        instanced ? gpu_particle_layout::QuadComponentsPerVertex : 1,
        gpu_gl::Float, 0, 0, nullptr);
    gl.VertexAttribDivisor(gpu_particle_layout::CornerAttributeIndex, 0);
    gl.ActiveTexture(gpu_gl::Texture0 + gpu_particle_layout::CurveTextureUnit);
    gl.BindTexture(gpu_gl::Texture2D, curves);
}
sf::Transform GpuTrackImpl::transform(const sf::Transform& host,
                                      sf::Vector2f& shapeScale) const {
    sf::Transform local;
    local.translate(definition.offset);
    local.rotate(sf::degrees(definition.rotationOffset));
    local.scale(definition.scale);
    shapeScale = {1, 1};
    if (definition.scaleMode == EmitterTrackParameters::ScaleMode::Local) {
        return withoutScale(host) * local;
    }
    const sf::Transform full = host * local;
    if (definition.scaleMode == EmitterTrackParameters::ScaleMode::Shape) {
        const float* matrix = full.getMatrix();
        shapeScale = {std::hypot(matrix[0], matrix[1]),
                      std::hypot(matrix[4], matrix[5])};
        return withoutScale(full);
    }
    return full;
}
void GpuTrackImpl::reset() {
    births = 0;
    credit = 0;
    pending = 0;
    initializedResident = false;
    lastBirth = 0;
    warmTime = 0;
    warmEnd =
        definition.prewarm && definition.loop
            ? std::ceil(std::max(definition.duration, definition.lifetime.y) /
                        definition.duration) *
                  definition.duration
            : 0;
    simulate(0, 0, {}, sf::Transform::Identity, 1, false, true);
}
void GpuTrackImpl::simulate(float delta, int count, const sf::Vector2f& motion,
                            const sf::Transform& host, int seed, bool draining,
                            bool resetState) {
    GpuApi& gl = resources->api;
    const unsigned int program = resources->updateProgram;
    gl.UseProgram(program);
    bindState(false);
    const auto scalar = [&](const char* name, float value) {
        gl.Uniform1f(resources->uniform(program, name), value);
    };
    const auto vector = [&](const char* name, sf::Vector2f value) {
        gl.Uniform2f(resources->uniform(program, name), value.x, value.y);
    };
    scalar("uDelta", delta);
    scalar("uReset", resetState ? 1.f : 0.f);
    scalar("uSeed", static_cast<float>(
                        (static_cast<std::uint64_t>(seed) + seedSalt_ +
                         (births / definition.capacity) * GenerationStride) %
                        gpu_particle_random::SeedModulus));
    scalar("uCapacity", static_cast<float>(definition.capacity));
    scalar("uBirthStart", static_cast<float>(births % definition.capacity));
    scalar("uBirthCount",
           static_cast<float>(std::min(count, definition.capacity)));
    scalar("uResident", definition.resident ? 1.f : 0.f);
    scalar("uLoop", definition.loop ? 1.f : 0.f);
    scalar("uDraining", draining ? 1.f : 0.f);
    scalar("uWorld", definition.world ? 1.f : 0.f);
    scalar("uShape", static_cast<float>(definition.shape));
    scalar("uRadius", definition.radius);
    scalar("uInnerRadius", definition.innerRadius);
    scalar("uDirection", definition.direction);
    scalar("uSpread", definition.spread);
    scalar("uDamping", definition.damping);
    scalar("uRadial", definition.radialAcceleration);
    scalar("uTangential", definition.tangentialAcceleration);
    vector("uLifetime", definition.lifetime);
    vector("uSpeed", definition.speed);
    vector("uSizeMin", definition.sizeMin);
    vector("uSizeMax", definition.sizeMax);
    vector("uRotation", definition.rotation);
    vector("uAngular", definition.angularVelocity);
    vector("uExtent", definition.extent);
    vector("uGravity", definition.gravity);
    vector("uMotion", motion);
    sf::Vector2f shapeScale;
    const sf::Transform pose = transform(host, shapeScale);
    vector("uShapeScale", shapeScale);
    gl.UniformMatrix4fv(resources->uniform(program, "uHost"), 1, 0,
                        pose.getMatrix());
    const auto& low = definition.colourMin;
    const auto& high = definition.colourMax;
    gl.Uniform4f(resources->uniform(program, "uColourMin"), low[0], low[1],
                 low[2], low[3]);
    gl.Uniform4f(resources->uniform(program, "uColourMax"), high[0], high[1],
                 high[2], high[3]);
    gl.Uniform1i(resources->uniform(program, "uCurves"),
                 gpu_particle_layout::CurveTextureUnit);
    gl.BindBufferBase(gpu_gl::TransformFeedbackBuffer,
                      gpu_particle_layout::FeedbackBindingIndex,
                      buffers[1 - input]);
    gl.Enable(gpu_gl::RasterizerDiscard);
    gl.BeginTransformFeedback(gpu_gl::Points);
    gl.DrawArrays(gpu_gl::Points, 0, definition.capacity);
    gl.EndTransformFeedback();
    gl.Disable(gpu_gl::RasterizerDiscard);
    gl.BindBufferBase(gpu_gl::TransformFeedbackBuffer,
                      gpu_particle_layout::FeedbackBindingIndex, 0);
    input = 1 - input;
    births += static_cast<std::uint64_t>(std::min(count, definition.capacity));
}
void GpuTrackImpl::step(float previous, float current, float delta,
                        float distance, const sf::Vector2f& motion,
                        const sf::Transform& host, int seed, bool draining) {
    int count = pending;
    pending = 0;
    const float from = previous - definition.delay;
    const float to = current - definition.delay;
    if (!draining && to >= 0) {
        if (definition.resident) {
            if (!initializedResident) {
                count += definition.count;
                initializedResident = true;
            }
        } else {
            const float activeTime =
                definition.loop
                    ? std::max(0.f, to) - std::max(0.f, from)
                    : std::clamp(to, 0.f, definition.duration) -
                          std::clamp(from, 0.f, definition.duration);
            if (activeTime > 0) {
                credit += static_cast<double>(activeTime) * definition.rate +
                          static_cast<double>(distance) *
                              definition.distanceRate * activeTime / delta;
                const double generated = std::floor(credit);
                count += static_cast<int>(std::min(
                    generated, static_cast<double>(definition.capacity)));
                credit -= generated;
            }
            for (const EmitterBurst& burst : definition.bursts) {
                const auto eventsBefore = [&](double time) {
                    const double cycles = definition.loop
                                              ? std::floor(std::max(0.0, time) /
                                                           definition.duration)
                                              : 0;
                    const double within =
                        definition.loop
                            ? std::max(0.0, time) - cycles * definition.duration
                            : std::clamp(
                                  time, 0.0,
                                  static_cast<double>(definition.duration));
                    const auto events = [&](double end) {
                        return burst.interval > 0
                                   ? std::clamp(
                                         std::ceil((end - burst.time) /
                                                   burst.interval),
                                         0.0, static_cast<double>(burst.cycles))
                                   : (end > burst.time ? 1.0 : 0.0);
                    };
                    return cycles * events(definition.duration) +
                           events(within);
                };
                const double emitted =
                    std::max(0.0, eventsBefore(to) - eventsBefore(from));
                count = static_cast<int>(
                    std::min(static_cast<double>(definition.capacity),
                             count + emitted * burst.count));
            }
        }
    }
    count = std::min(count, definition.capacity);
    if (count > 0) {
        lastBirth = current;
    }
    simulate(delta, count, motion, host, seed, draining, false);
}
void GpuTrackImpl::draw(sf::RenderTarget& target,
                        const sf::RenderStates& states,
                        const sf::Transform& host, const sf::Color& colour) {
    GpuApi& gl = resources->api;
    const unsigned int program = resources->drawProgram;
    gl.UseProgram(program);
    bindState(true);
    gl.Uniform4f(resources->uniform(program, "uTint"), colour.r / 255.f,
                 colour.g / 255.f, colour.b / 255.f, colour.a / 255.f);
    gl.ActiveTexture(gpu_gl::Texture0 + gpu_particle_layout::ImageTextureUnit);
    gl.BindTexture(gpu_gl::Texture2D, texture->getNativeHandle());
    gl.Uniform1i(resources->uniform(program, "uImage"),
                 gpu_particle_layout::ImageTextureUnit);
    gl.Uniform1i(resources->uniform(program, "uCurves"),
                 gpu_particle_layout::CurveTextureUnit);
    const sf::Transform projection =
        target.getView().getTransform() * states.transform;
    sf::Vector2f unused;
    const sf::Transform pose = transform(host, unused);
    gl.UniformMatrix4fv(resources->uniform(program, "uProjection"), 1, 0,
                        projection.getMatrix());
    gl.UniformMatrix4fv(resources->uniform(program, "uHost"), 1, 0,
                        pose.getMatrix());
    const auto scalar = [&](const char* name, float value) {
        gl.Uniform1f(resources->uniform(program, name), value);
    };
    const SceneLightingContext* lighting = currentSceneLighting();
    const bool lit = lighting && lighting->enabled;
    scalar("sceneLightingEnabled", lit ? 1.0f : 0.0f);
    gl.Uniform2f(resources->uniform(program, "sceneTargetSize"),
                 static_cast<float>(target.getSize().x),
                 static_cast<float>(target.getSize().y));
    if (lit) {
        gl.ActiveTexture(gpu_gl::Texture0 +
                         gpu_particle_layout::AmbientLightTextureUnit);
        gl.BindTexture(gpu_gl::Texture2D,
                       lighting->ambientLight->getNativeHandle());
        gl.Uniform1i(resources->uniform(program, "sceneAmbientLight"),
                     gpu_particle_layout::AmbientLightTextureUnit);
        gl.ActiveTexture(gpu_gl::Texture0 +
                         gpu_particle_layout::DirectLightTextureUnit);
        gl.BindTexture(gpu_gl::Texture2D,
                       lighting->directLight->getNativeHandle());
        gl.Uniform1i(resources->uniform(program, "sceneDirectLight"),
                     gpu_particle_layout::DirectLightTextureUnit);
    }
    scalar("uWorld", definition.world ? 1.f : 0.f);
    scalar("uFrameRate", definition.frameRate);
    scalar("uFrameCount", static_cast<float>(definition.frameCount));
    scalar("uFrameLoop", definition.frameLoop ? 1.f : 0.f);
    scalar("uRandomFrame", definition.randomStartFrame ? 1.f : 0.f);
    gl.Uniform2f(resources->uniform(program, "uGrid"),
                 static_cast<float>(definition.columns),
                 static_cast<float>(definition.rows));
    const sf::Vector2u size = texture->getSize();
    const auto& rect = definition.textureRect;
    gl.Uniform4f(resources->uniform(program, "uRect"),
                 static_cast<float>(rect.position.x) / size.x,
                 static_cast<float>(rect.position.y) / size.y,
                 static_cast<float>(rect.size.x) / size.x,
                 static_cast<float>(rect.size.y) / size.y);
    gl.Enable(gpu_gl::Blend);
    gl.BlendEquationSeparate(gpu_gl::FuncAdd, gpu_gl::FuncAdd);
    gl.BlendFuncSeparate(
        gpu_gl::SourceAlpha,
        definition.additive ? gpu_gl::One : gpu_gl::OneMinusSourceAlpha,
        gpu_gl::One, gpu_gl::OneMinusSourceAlpha);
    const auto viewport = target.getViewport(target.getView());
    gl.Viewport(viewport.position.x,
                static_cast<int>(target.getSize().y) - viewport.position.y -
                    viewport.size.y,
                viewport.size.x, viewport.size.y);
    gl.DrawArraysInstanced(gpu_gl::Triangles, 0,
                           gpu_particle_layout::QuadVertexCount,
                           definition.capacity);
}
}  // namespace ludork::runtime::graphics
