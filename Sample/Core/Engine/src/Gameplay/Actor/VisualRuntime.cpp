#include "VisualRuntime.hpp"

#include <ConcurrentResourceCache.hpp>
#include <Utils/ShaderLoader.hpp>

#include <iostream>
#include <stdexcept>

namespace ludork::engine::actor_impl {

namespace {

std::unique_ptr<sf::Texture>& blankTextureStorage() {
    static std::unique_ptr<sf::Texture> texture;
    return texture;
}

ludork::core::ConcurrentResourceCache<sf::Shader, true>& shaderCache() {
    static ludork::core::ConcurrentResourceCache<sf::Shader, true> cache;
    return cache;
}

}  // namespace

const sf::Texture& textureOrBlank(const std::shared_ptr<sf::Texture>& texture) {
    if (texture) {
        return *texture;
    }
    std::unique_ptr<sf::Texture>& blank = blankTextureStorage();
    if (blank == nullptr) {
        blank = std::make_unique<sf::Texture>();
    }
    return *blank;
}

ShaderResult loadShader(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    try {
        return {shaderCache().getOrLoad(
                    path,
                    [&path]() {
                        ShaderLoadResult result = ShaderLoader::load(
                            path, sf::Shader::Type::Fragment);
                        if (!result) {
                            throw std::runtime_error(result.error);
                        }
                        return std::move(result.shader);
                    }),
                false};
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return {nullptr, true};
    }
}

sf::IntRect nextAnimationRect(const sf::IntRect& current,
                              unsigned int textureWidth) {
    if (textureWidth == 0) {
        return current;
    }
    return {
        {(current.position.x + current.size.x) % static_cast<int>(textureWidth),
         current.position.y},
        current.size};
}

void shutdownVisualResources() noexcept {
    shaderCache().clear();
    blankTextureStorage().reset();
}

}  // namespace ludork::engine::actor_impl
