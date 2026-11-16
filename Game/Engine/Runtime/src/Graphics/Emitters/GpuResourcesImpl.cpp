#include "GpuResourcesImpl.hpp"
#include "GpuParticleLayout.hpp"
#include <Runtime/Graphics/GpuEmitterShaders.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::runtime::graphics {

GpuResourcesImpl::GpuResourcesImpl(std::shared_ptr<sf::rhi::Device> graphics)
    : device(std::move(graphics)) {
    const sf::rhi::Capabilities capabilities = device->capabilities();
    if (!capabilities.vertexTransforms || !capabilities.instancedDrawing ||
        !capabilities.floatTextures) {
        throw std::runtime_error(
            "GPU Emitter requires vertex transforms, instanced drawing and "
            "floating-point textures");
    }
    const auto programs = GpuEmitterShaders::programs();
    updateShader = ShaderCache::instantiate(
        programs[static_cast<std::size_t>(GpuEmitterShaders::Kind::Update)]);
    drawShader = ShaderCache::instantiate(
        programs[static_cast<std::size_t>(GpuEmitterShaders::Kind::Draw)]);
    updateProgram = sf::rhi::getShader(*updateShader);
    drawProgram = sf::rhi::getShader(*drawShader);
    if (!updateProgram || !drawProgram) {
        throw std::runtime_error(
            "GPU Emitter shader resources are unavailable");
    }
    const float vertices[gpu_particle_layout::QuadVertexCount *
                         gpu_particle_layout::QuadComponentsPerVertex] = {
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f,  0.5f,
        -0.5f, -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f};
    corners =
        device->createBuffer(sizeof(vertices), sf::rhi::BufferUsage::Static);
    if (!corners || !corners->upload(vertices, sizeof(vertices))) {
        throw std::runtime_error("Cannot create GPU Emitter quad vertices");
    }
    renderer = device->getCacheIdentity().device;
    if (renderer.empty()) {
        switch (device->backend()) {
            case sf::GraphicsBackend::DirectX:
                renderer = "DirectX";
                break;
            case sf::GraphicsBackend::Metal:
                renderer = "Metal";
                break;
            case sf::GraphicsBackend::OpenGLES:
                renderer = "OpenGL ES";
                break;
            case sf::GraphicsBackend::OpenGL:
                renderer = "OpenGL";
                break;
            case sf::GraphicsBackend::Vulkan:
                renderer = "Vulkan";
                break;
            default:
                renderer = "Unknown";
                break;
        }
    }
}

void GpuResourcesImpl::reserveIndices(int capacity) {
    if (capacity <= indexCapacity) {
        return;
    }
    std::vector<float> values(static_cast<std::size_t>(capacity));
    for (int index = 0; index < capacity; ++index) {
        values[static_cast<std::size_t>(index)] = static_cast<float>(index);
    }
    const std::size_t bytes = values.size() * sizeof(float);
    auto buffer = device->createBuffer(bytes, sf::rhi::BufferUsage::Static);
    if (!buffer || !buffer->upload(values.data(), bytes)) {
        throw std::runtime_error("Cannot create GPU Emitter particle indices");
    }
    indices = std::move(buffer);
    indexCapacity = capacity;
}

std::shared_ptr<GpuResourcesImpl> GpuResourcesImpl::acquire() {
    static thread_local std::weak_ptr<GpuResourcesImpl> resources;
    const auto device = sf::rhi::getDevice();
    if (!device) {
        throw std::runtime_error("GPU Emitter requires a graphics device");
    }
    auto result = resources.lock();
    if (!result || result->device != device) {
        result = std::make_shared<GpuResourcesImpl>(device);
        resources = result;
    }
    return result;
}

}  // namespace ludork::runtime::graphics
