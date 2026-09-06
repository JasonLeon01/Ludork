#pragma once

#include <SFML/Graphics/Shader.hpp>

#include <memory>
#include <string>

namespace ludork::engine::actor_impl {

class VisualImpl {
public:
    void ensureShaderLoaded(const std::string& path);
    void invalidateShader();
    std::shared_ptr<sf::Shader> getShader() const;
    bool hasShaderError() const;
    bool advanceAnimation(float deltaTime, float interval);

private:
    float switchTimer_ = 0.0f;
    std::shared_ptr<sf::Shader> shader_;
    bool shaderError_ = false;
    std::string loadedShaderPath_;
};

}  // namespace ludork::engine::actor_impl
