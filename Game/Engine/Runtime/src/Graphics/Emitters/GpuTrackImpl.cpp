#include "GpuParticleLayout.hpp"
#include "GpuParticleRandom.hpp"
#include "GpuTrackImpl.hpp"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Glsl.hpp>
#include <Runtime/AssetStore.hpp>
#include <Runtime/AssetInputStream.hpp>
#include <Runtime/ConcurrentResourceCache.hpp>
#include <Runtime/Graphics/GpuEmitterCurveLayout.hpp>
#include <Runtime/Graphics/GpuEmitterShaders.hpp>
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
    resources->reserveIndices(definition.capacity);
    const std::size_t stateBytes =
        static_cast<std::size_t>(definition.capacity) *
        gpu_particle_layout::StateStrideBytes;
    for (auto& buffer : buffers) {
        buffer = resources->device->createBuffer(stateBytes,
                                                 sf::rhi::BufferUsage::Dynamic);
        if (!buffer) {
            throw std::runtime_error("Cannot allocate GPU particle state");
        }
    }
    curves = resources->device->createFloatTexture(
        {emitter_curve_layout::SampleCount,
         emitter_curve_layout::TextureRowCount},
        definition.curveSamples.data());
    if (!curves) {
        throw std::runtime_error("Cannot create GPU particle curve texture");
    }
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
        throw std::invalid_argument("Particle textureRect exceeds texture: " +
                                    data.name);
    }
    reset();
}

GpuTrackImpl::~GpuTrackImpl() = default;

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
    sf::Shader& program = *resources->updateShader;
    resources->updateProgram->setSampler("uCurves", curves);
    const auto scalar = [&](const char* name, float value) {
        program.setUniform(name, value);
    };
    const auto vector = [&](const char* name, sf::Vector2f value) {
        program.setUniform(name, value);
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
    program.setUniform("uHost", sf::Glsl::Mat4(pose.getMatrix()));
    const auto& low = definition.colourMin;
    const auto& high = definition.colourMax;
    program.setUniform("uColourMin",
                       sf::Glsl::Vec4(low[0], low[1], low[2], low[3]));
    program.setUniform("uColourMax",
                       sf::Glsl::Vec4(high[0], high[1], high[2], high[3]));
    if (!resources->device->transformVertices(
            resources->updateProgram,
            {{buffers[input], 0}, {resources->indices, 0}},
            static_cast<std::size_t>(definition.capacity),
            buffers[1 - input])) {
        throw std::runtime_error("GPU particle simulation submission failed");
    }
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
    sf::Shader& program = *resources->drawShader;
    program.setUniform("uTint", sf::Glsl::Vec4(colour));
    resources->drawProgram->setSampler("uImage", sf::rhi::getTexture(*texture));
    resources->drawProgram->setSampler("uCurves", curves);
    const sf::Transform projection =
        target.getView().getTransform() * states.transform;
    sf::Vector2f unused;
    const sf::Transform pose = transform(host, unused);
    program.setUniform("uProjection", sf::Glsl::Mat4(projection.getMatrix()));
    program.setUniform("uHost", sf::Glsl::Mat4(pose.getMatrix()));
    const SceneLightingContext* lighting = currentSceneLighting();
    const bool lit = lighting && lighting->enabled;
    program.setUniform("sceneLightingEnabled", lit ? 1.0f : 0.0f);
    program.setUniform("sceneTargetSize", sf::Vector2f(target.getSize()));
    if (lit) {
        resources->drawProgram->setSampler(
            "sceneAmbientLight", sf::rhi::getTexture(*lighting->ambientLight));
        resources->drawProgram->setSampler(
            "sceneDirectLight", sf::rhi::getTexture(*lighting->directLight));
    }
    program.setUniform("uWorld", definition.world ? 1.f : 0.f);
    program.setUniform("uFrameRate", definition.frameRate);
    program.setUniform("uFrameCount",
                       static_cast<float>(definition.frameCount));
    program.setUniform("uFrameLoop", definition.frameLoop ? 1.f : 0.f);
    program.setUniform("uRandomFrame", definition.randomStartFrame ? 1.f : 0.f);
    program.setUniform("uGrid",
                       sf::Vector2f(static_cast<float>(definition.columns),
                                    static_cast<float>(definition.rows)));
    const sf::Vector2u size = texture->getSize();
    const auto& rect = definition.textureRect;
    program.setUniform(
        "uRect", sf::Glsl::Vec4(static_cast<float>(rect.position.x) / size.x,
                                static_cast<float>(rect.position.y) / size.y,
                                static_cast<float>(rect.size.x) / size.x,
                                static_cast<float>(rect.size.y) / size.y));
    const auto nativeTarget = sf::rhi::getRenderTarget(target);
    if (!nativeTarget) {
        throw std::runtime_error("GPU Emitter render target is unavailable");
    }
    sf::rhi::DrawSettings settings;
    settings.shader = resources->drawProgram;
    settings.blendMode = GpuEmitterShaders::blendMode(definition.additive);
    if (!nativeTarget->commands().drawVertices(
            sf::rhi::getTargetState(target),
            {{buffers[input], 0}, {resources->corners, 0}}, 0,
            gpu_particle_layout::QuadVertexCount,
            static_cast<std::size_t>(definition.capacity),
            sf::PrimitiveType::Triangles, settings)) {
        throw std::runtime_error("GPU particle drawing submission failed");
    }
}
}  // namespace ludork::runtime::graphics
