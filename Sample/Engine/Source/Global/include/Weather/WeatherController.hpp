#pragma once

#include <CoreMinimal.hpp>

#include <Camera.hpp>

#include <random>

BIND_ENUM()
enum class WeatherType {
    NONE = 0,
    RAIN = 1,
    STORM = 2,
    SNOW = 3,
};

BIND_CLASS()
class WeatherController {
public:
    BIND_METHOD()
    static void setWeather(WeatherType weatherType, float power, int maxCount);

    BIND_METHOD()
    static void clearWeather();

    BIND_METHOD()
    static void update(float deltaTime);

    BIND_METHOD()
    static void drawShaderOverlay(Camera& camera);

    BIND_METHOD(Pure = true)
    static WeatherType getWeatherType();

    static void shutdown() noexcept;

private:
    static void ensureShader();
    static void drawShaderOverlayTo(sf::RenderTexture& renderTexture);
    static sf::RenderTexture& ensureBuffer(const sf::Vector2u& size);
    static sf::Sprite& ensureBufferSprite();
    static float randomUnit();

    static WeatherType weatherType_;
    static float power_;
    static float maxCount_;
    static float time_;
    static float stormFlashCooldown_;
    static std::shared_ptr<sf::Shader> weatherShader_;
    static std::unique_ptr<sf::RenderTexture> weatherBuffer_;
    static std::optional<sf::Sprite> bufferSprite_;
    static std::mt19937 random_;
};
