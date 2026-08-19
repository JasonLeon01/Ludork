#pragma once

#include <BindAnnotations.hpp>
#include <Camera.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <SFML/Graphics.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

class Particle;
class ParticleBase;
class ParticleSystem;

BIND_MODULE_PROPERTY(name = "NONE", readonly = true)
extern const int WeatherTypeNone;

BIND_MODULE_PROPERTY(name = "RAIN", readonly = true)
extern const int WeatherTypeRain;

BIND_MODULE_PROPERTY(name = "STORM", readonly = true)
extern const int WeatherTypeStorm;

BIND_MODULE_PROPERTY(name = "SNOW", readonly = true)
extern const int WeatherTypeSnow;

BIND_FUNCTION(name = "coerce")
int coerceWeatherType(const RuntimeValue& value);

BIND_FUNCTION(name = "dropBoxItems")
std::vector<std::string> getWeatherTypeDropBoxItems();

BIND_CLASS()
class WeatherController {
public:
    BIND_METHOD()
    static void setWeather(const RuntimeValue& weatherType, float power,
                           int maxCount);

    BIND_METHOD()
    static void clearWeather();

    BIND_METHOD()
    static void registerParticleSystem(
        const std::shared_ptr<ParticleSystem>& particleSystem);

    BIND_METHOD()
    static void update(float deltaTime);

    BIND_METHOD()
    static void drawShaderOverlay(Camera& camera);

    BIND_METHOD(Pure = true)
    static int getWeatherType();

    BIND_IGNORE()
    static void shutdown() noexcept;

private:
    static void ensureShader();
    static void drawShaderOverlayTo(sf::RenderTexture& renderTexture);
    static sf::RenderTexture& ensureBuffer(const sf::Vector2u& size);
    static sf::Sprite& ensureBufferSprite();
    static void clearParticles();
    static void spawnParticles();
    static int particleCount();
    static std::function<void(float, float, ParticleBase*)> buildMover(
        float width, float height);
    static float randomUnit();

    static int weatherType_;
    static float power_;
    static float maxCount_;
    static float time_;
    static float stormFlashCooldown_;
    static std::shared_ptr<sf::Shader> weatherShader_;
    static bool shaderFailed_;
    static bool useParticles_;
    static std::unique_ptr<sf::RenderTexture> weatherBuffer_;
    static std::optional<sf::Sprite> bufferSprite_;
    static std::vector<std::shared_ptr<Particle>> particles_;
    static std::weak_ptr<ParticleSystem> particleSystem_;
    static std::mt19937 random_;
};
