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
        const ShaderLoader::ShaderSourceResult vertex =
            ShaderLoader::readSource(
                "/Game/Assets/Shaders/Global/ActorPixelShatter.vert");
        ShaderLoader::ShaderSourceResult fragment = ShaderLoader::readSource(
            "/Game/Assets/Shaders/Global/ActorPixelShatter.frag");
        if (!vertex) {
            throw std::runtime_error(vertex.error);
        }
        if (!fragment) {
            throw std::runtime_error(fragment.error);
        }
        const std::string marker = "// LUDORK_SCENE_LIGHTING";
        const std::size_t position = fragment.source.find(marker);
        if (position == std::string::npos) {
            throw std::runtime_error(
                "Actor pixel shatter shader requires the scene lighting "
                "marker");
        }
        fragment.source.replace(position, marker.size(),
                                ludork::runtime::graphics::sceneLightingGlsl());
        const std::shared_ptr<sf::Shader> shader =
            std::make_shared<sf::Shader>();
        if (!shader->loadFromMemory(vertex.source, fragment.source)) {
            throw std::runtime_error(
                "Could not compile Actor pixel shatter shader");
        }
        return shader;
    });
}

void ActorPixelShatterShader::bindLighting(
    const std::shared_ptr<sf::Shader>& shader) {
    if (!shader) {
        throw std::invalid_argument("Actor pixel shatter requires a shader");
    }
    ludork::runtime::graphics::bindSceneLighting(*shader);
}
