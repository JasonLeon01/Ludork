#pragma once

#include <BindAnnotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <SFML/Graphics.hpp>

#include <memory>
#include <optional>
#include <string>

BIND_CLASS()
class FogController {
public:
    BIND_METHOD()
    static void applyFromMapData(const RuntimeValue::Map& mapData);

    BIND_METHOD()
    static void clearFog();

    BIND_METHOD()
    static void update(float deltaTime);

    BIND_METHOD()
    static void drawOverlay();

    BIND_IGNORE()
    static void shutdown() noexcept;

private:
    static bool loadFogTexture();
    static void ensureShader();
    static void ensureFallbackSprite();
    static void updateFallbackSprite();
    static void drawFallbackOverlay(sf::RenderTexture& canvas);
    static void drawShaderOverlay(sf::RenderTexture& canvas);
    static sf::RenderTexture& ensureBuffer(const sf::Vector2u& size);
    static sf::Sprite& ensureBufferSprite();

    static std::string graphic_;
    static float power_;
    static sf::Vector2f scroll_;
    static float distort_;
    static sf::Vector2f offset_;
    static float time_;
    static bool active_;
    static std::shared_ptr<sf::Texture> fogTexture_;
    static std::optional<sf::Sprite> fogSprite_;
    static std::shared_ptr<sf::Shader> fogShader_;
    static bool shaderFailed_;
    static std::unique_ptr<sf::RenderTexture> fogBuffer_;
    static std::optional<sf::Sprite> bufferSprite_;
};
