#pragma once

#include <CoreMinimal.hpp>

class Camera;

BIND_CLASS()
class FogController {
public:
    BIND_METHOD()
    static void applyFromMapData(const RuntimeValue::Map& mapData);

    BIND_METHOD(metadata = false)
    static void applyWorldFromMapData(const RuntimeValue::Map& mapData);

    BIND_METHOD(metadata = false)
    static void setWorldRegionFog(const std::string& key,
                                  const sf::IntRect& cellRect,
                                  const std::string& graphic, float power,
                                  float scrollX, float scrollY, float distort);

    BIND_METHOD(metadata = false)
    static void removeWorldRegionFog(const std::string& key);

    BIND_METHOD(metadata = false)
    static void drawWorldOverlay(Camera& camera);

    BIND_METHOD()
    static void clearFog();

    BIND_METHOD()
    static void update(float deltaTime);

    BIND_METHOD()
    static void drawOverlay();

    static void shutdown() noexcept;

private:
    static bool loadFogTexture();
    static void ensureShader();
    static void drawShaderOverlay(sf::RenderTexture& canvas);
    static sf::RenderTexture& ensureBuffer(const sf::Vector2u& size);
    static sf::Sprite& ensureBufferSprite();
};
