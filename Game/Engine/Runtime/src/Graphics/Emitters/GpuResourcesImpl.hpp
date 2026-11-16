#pragma once

#include <SFML/Graphics/RHI/Device.hpp>
#include <SFML/Graphics/Shader.hpp>

#include <memory>
#include <string>

namespace ludork::runtime::graphics {

class GpuResourcesImpl {
public:
    std::shared_ptr<sf::rhi::Device> device;
    std::shared_ptr<sf::Shader> updateShader;
    std::shared_ptr<sf::Shader> drawShader;
    std::shared_ptr<sf::rhi::ShaderProgram> updateProgram;
    std::shared_ptr<sf::rhi::ShaderProgram> drawProgram;
    std::shared_ptr<sf::rhi::Buffer> corners;
    std::shared_ptr<sf::rhi::Buffer> indices;
    int indexCapacity = 0;
    std::string renderer;

    explicit GpuResourcesImpl(std::shared_ptr<sf::rhi::Device> device);
    void reserveIndices(int capacity);
    static std::shared_ptr<GpuResourcesImpl> acquire();
};

}  // namespace ludork::runtime::graphics
