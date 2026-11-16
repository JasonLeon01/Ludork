#include <Graphics/ActorPixelShatterShader.hpp>

#include <Runtime/ConcurrentResourceCache.hpp>
#include <Runtime/Graphics/SceneLighting.hpp>
#include <Utils/ShaderLoader.hpp>

#include <stdexcept>

namespace {

ludork::runtime::ConcurrentResourceCache<sf::Shader>& shatterShaders() {
    static ludork::runtime::ConcurrentResourceCache<sf::Shader> cache;
    return cache;
}

}  // namespace

std::shared_ptr<sf::Shader> ActorPixelShatterShader::load() {
    return shatterShaders().getOrLoad("ActorPixelShatter", []() {
        auto result = ShaderLoader::load(
            "/Game/Assets/Shaders/Global/ActorPixelShatter.vert",
            "/Game/Assets/Shaders/Global/ActorPixelShatter.frag");
        if (!result) {
            throw std::runtime_error(result.error);
        }
        return result.shader;
    });
}

void ActorPixelShatterShader::bindLighting(
    const std::shared_ptr<sf::Shader>& shader) {
    if (!shader) {
        throw std::invalid_argument("Actor pixel shatter requires a shader");
    }
    ludork::runtime::graphics::bindSceneLighting(*shader);
}
