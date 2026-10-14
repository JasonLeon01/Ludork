#pragma once

#include <RuntimeApi.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <string>

namespace ludork::runtime::graphics {

struct SceneLightingContext {
    const sf::Texture* ambientLight = nullptr;
    const sf::Texture* directLight = nullptr;
    sf::Vector2u targetSize{1, 1};
    sf::FloatRect worldView{{0.0f, 0.0f}, {1.0f, 1.0f}};
    float viewRotation = 0.0f;
    bool enabled = false;
};

LUDORK_RUNTIME_API const SceneLightingContext* currentSceneLighting();
LUDORK_RUNTIME_API std::string sceneLightingGlsl(
    const std::string& textureFunction = "texture2D");
LUDORK_RUNTIME_API void bindSceneLighting(sf::Shader& shader);
LUDORK_RUNTIME_API std::shared_ptr<sf::Shader> sceneLightingShader();

}  // namespace ludork::runtime::graphics
