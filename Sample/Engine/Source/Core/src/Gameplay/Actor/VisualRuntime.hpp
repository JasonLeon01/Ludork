#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <string>

namespace ludork::engine::actor_impl {

struct ShaderResult {
    std::shared_ptr<sf::Shader> shader;
    bool failed = false;
};

const sf::Texture& textureOrBlank(const std::shared_ptr<sf::Texture>& texture);
ShaderResult loadShader(const std::string& path);
sf::IntRect nextAnimationRect(const sf::IntRect& current,
                              unsigned int textureWidth);
void shutdownVisualResources() noexcept;

}  // namespace ludork::engine::actor_impl
