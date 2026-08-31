#pragma once

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ludork::global::fog_controller_impl {

struct FogRenderRuntime {
    std::string graphic;
    float power = 0.0f;
    sf::Vector2f scroll;
    float distort = 0.0f;
    sf::Vector2f offset;
    float time = 0.0f;
    bool active = false;
    std::shared_ptr<sf::Texture> fogTexture;
    std::shared_ptr<sf::Shader> fogShader;
    std::unique_ptr<sf::RenderTexture> fogBuffer;
    std::optional<sf::Sprite> bufferSprite;
};

FogRenderRuntime& fogRenderRuntime();

}  // namespace ludork::global::fog_controller_impl
