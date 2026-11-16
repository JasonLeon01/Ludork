#include "GpuStatisticsImpl.hpp"
#include "GpuParticleLayout.hpp"
#include <Runtime/Graphics/GpuEmitterShaders.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ludork::runtime::graphics {
namespace {

sf::rhi::TargetState targetState(const sf::rhi::RenderTarget& target) {
    const auto size = target.getSize();
    return {size,
            {{0, 0}, sf::Vector2i(size)},
            {{0, 0}, sf::Vector2i(size)},
            sf::Transform::Identity,
            false};
}

}  // namespace

GpuStatisticsImpl::GpuStatisticsImpl(
    std::shared_ptr<GpuResourcesImpl> resources)
    : resources_(std::move(resources)) {
    if (!available()) {
        return;
    }
    const auto programs = GpuEmitterShaders::programs();
    maskShader_ = ShaderCache::instantiate(
        programs[static_cast<std::size_t>(GpuEmitterShaders::Kind::Mask)]);
    reductionShader_ = ShaderCache::instantiate(
        programs[static_cast<std::size_t>(GpuEmitterShaders::Kind::Reduction)]);
    maskProgram_ = sf::rhi::getShader(*maskShader_);
    reductionProgram_ = sf::rhi::getShader(*reductionShader_);
    if (!maskProgram_ || !reductionProgram_) {
        throw std::runtime_error(
            "GPU particle statistics shaders are unavailable");
    }
}

GpuStatisticsImpl::~GpuStatisticsImpl() = default;

bool GpuStatisticsImpl::available() const {
    return resources_ &&
           resources_->device->capabilities().asynchronousReadback;
}

void GpuStatisticsImpl::reserve(int size) {
    if (size_ == size) {
        return;
    }
    std::vector<std::shared_ptr<sf::rhi::Texture>> textures;
    std::vector<std::shared_ptr<sf::rhi::RenderTarget>> targets;
    for (int dimension = size; dimension >= 1; dimension /= 2) {
        const unsigned int side = static_cast<unsigned int>(dimension);
        auto texture = resources_->device->createTexture({side, side});
        auto target =
            texture ? resources_->device->createRenderTarget(texture) : nullptr;
        if (!target) {
            throw std::runtime_error(
                "Cannot allocate GPU particle statistics reduction targets");
        }
        textures.push_back(std::move(texture));
        targets.push_back(std::move(target));
    }
    textures_ = std::move(textures);
    targets_ = std::move(targets);
    size_ = size;
}

bool GpuStatisticsImpl::submit(const std::vector<StateBuffer>& stateBuffers,
                               double simulationTime, bool force) {
    if (!available() || readback_ ||
        (!force && lastSubmitTime_.has_value() &&
         simulationTime >= *lastSubmitTime_ &&
         simulationTime - *lastSubmitTime_ < MinimumSampleInterval)) {
        return false;
    }
    if (!std::isfinite(simulationTime) || simulationTime < 0) {
        throw std::invalid_argument(
            "GPU statistics sample time must be finite and nonnegative");
    }
    std::uint64_t total = 0;
    int largestTrack = 0;
    for (const auto& [buffer, capacity] : stateBuffers) {
        if (!buffer || capacity < 0) {
            throw std::invalid_argument(
                "GPU statistics requires valid state buffers");
        }
        total += static_cast<std::uint64_t>(capacity);
        largestTrack = std::max(largestTrack, capacity);
    }
    if (total == 0) {
        zeroSample_ = Sample{0, simulationTime};
        lastSubmitTime_ = simulationTime;
        return true;
    }
    const auto maximum = resources_->device->capabilities().maximumTextureSize;
    unsigned int side = 1;
    while (static_cast<std::uint64_t>(side) * side < total &&
           side <= maximum / 2) {
        side *= 2;
    }
    if (side > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        total > std::numeric_limits<std::uint32_t>::max() ||
        static_cast<std::uint64_t>(side) * side < total) {
        return false;
    }
    reserve(static_cast<int>(side));
    resources_->reserveIndices(largestTrack);
    auto& maskTarget = *targets_.front();
    const auto maskState = targetState(maskTarget);
    maskTarget.commands().clear(maskState, sf::Color::Transparent);
    maskShader_->setUniform("side", static_cast<float>(side));
    sf::rhi::DrawSettings settings;
    settings.blendMode = sf::BlendNone;
    settings.shader = maskProgram_;
    std::uint64_t offset = 0;
    for (const auto& [buffer, capacity] : stateBuffers) {
        maskShader_->setUniform(
            "baseSlot", sf::Vector2f(static_cast<float>(offset % side),
                                     static_cast<float>(offset / side)));
        if (!maskTarget.commands().drawVertices(
                maskState, {{buffer, 0}, {resources_->indices, 0}}, 0,
                static_cast<std::size_t>(capacity), 1,
                sf::PrimitiveType::Points, settings)) {
            throw std::runtime_error(
                "GPU particle statistics mask submission failed");
        }
        offset += static_cast<std::uint64_t>(capacity);
    }
    maskTarget.finish();
    settings.shader = reductionProgram_;
    for (std::size_t level = 1; level < targets_.size(); ++level) {
        auto& target = *targets_[level];
        reductionProgram_->setSampler("source", textures_[level - 1]);
        reductionShader_->setUniform(
            "side", static_cast<float>(textures_[level - 1]->getSize().x));
        if (!target.commands().drawVertices(
                targetState(target), {{resources_->corners, 0}}, 0,
                gpu_particle_layout::QuadVertexCount, 1,
                sf::PrimitiveType::Triangles, settings)) {
            throw std::runtime_error(
                "GPU particle statistics reduction submission failed");
        }
        target.finish();
    }
    readback_ = resources_->device->readTextureAsync(textures_.back(),
                                                     {{0, 0}, {1, 1}});
    if (!readback_ || !resources_->device->submit()) {
        readback_.reset();
        throw std::runtime_error(
            "Cannot submit asynchronous GPU particle statistics readback");
    }
    pendingTime_ = simulationTime;
    lastSubmitTime_ = simulationTime;
    return true;
}

std::optional<GpuStatisticsImpl::Sample> GpuStatisticsImpl::poll() {
    if (zeroSample_) {
        const auto result = zeroSample_;
        zeroSample_.reset();
        return result;
    }
    if (!readback_) {
        return std::nullopt;
    }
    const auto status = readback_->poll();
    if (status == sf::rhi::ReadbackStatus::Pending) {
        return std::nullopt;
    }
    if (status == sf::rhi::ReadbackStatus::Failed ||
        readback_->data().size() != sizeof(std::uint32_t)) {
        readback_.reset();
        throw std::runtime_error(
            "Asynchronous GPU particle statistics readback failed");
    }
    const auto& bytes = readback_->data();
    const std::uint32_t value = static_cast<std::uint32_t>(bytes[0]) |
                                (static_cast<std::uint32_t>(bytes[1]) << 8u) |
                                (static_cast<std::uint32_t>(bytes[2]) << 16u) |
                                (static_cast<std::uint32_t>(bytes[3]) << 24u);
    readback_.reset();
    return Sample{value, pendingTime_};
}

}  // namespace ludork::runtime::graphics
