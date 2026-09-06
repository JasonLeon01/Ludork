#include "VisualImpl.hpp"

#include "Graphics/SpriteVisuals.hpp"

namespace ludork::engine::actor_impl {

void VisualImpl::ensureShaderLoaded(const std::string& path) {
    if (loadedShaderPath_ == path) {
        return;
    }
    loadedShaderPath_ = path;
    shader_.reset();
    shaderError_ = false;
    if (path.empty()) {
        return;
    }
    const ludork::engine::sprite_visuals::ShaderResult result =
        ludork::engine::sprite_visuals::loadShader(path);
    shader_ = result.shader;
    shaderError_ = result.failed;
}

void VisualImpl::invalidateShader() {
    loadedShaderPath_.clear();
}

std::shared_ptr<sf::Shader> VisualImpl::getShader() const {
    return shader_;
}

bool VisualImpl::hasShaderError() const {
    return shaderError_;
}

bool VisualImpl::advanceAnimation(float deltaTime, float interval) {
    switchTimer_ += deltaTime;
    if (switchTimer_ < interval) {
        return false;
    }
    switchTimer_ = 0.0f;
    return true;
}

}  // namespace ludork::engine::actor_impl
