#pragma once

#include <SFML/Graphics/Shader.hpp>
#include <memory>

namespace ludork::global::system_impl {

class GraphicsShaderSink {
public:
    virtual ~GraphicsShaderSink() = default;
    virtual void addEffectShader(const std::shared_ptr<sf::Shader>& shader) = 0;
    virtual void removeEffectShader(
        const std::shared_ptr<sf::Shader>& shader) = 0;
};

}  // namespace ludork::global::system_impl
