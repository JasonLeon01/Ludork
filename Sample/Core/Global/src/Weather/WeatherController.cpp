#include <Weather/WeatherController.hpp>

#include <Camera.hpp>
#include <Manager/ShaderManager.hpp>
#include <Particles/Particle.hpp>
#include <Particles/ParticleSystem.hpp>
#include <Runtime/EngineState.hpp>
#include <System.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <string_view>

const int WeatherTypeNone = 0;
const int WeatherTypeRain = 1;
const int WeatherTypeStorm = 2;
const int WeatherTypeSnow = 3;

namespace {
constexpr std::string_view WeatherParticlePath =
    "Assets/Icons/Potion-1-1-1-1.png";
const sf::Color StormFlashColour{210, 220, 255, 120};

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

int checkedWeatherType(std::int64_t value) {
    return value >= WeatherTypeNone && value <= WeatherTypeSnow
               ? static_cast<int>(value)
               : WeatherTypeNone;
}

int weatherTypeByName(const std::string& value) {
    static const std::array<std::pair<std::string_view, int>, 4> entries{{
        {"NONE", WeatherTypeNone},
        {"RAIN", WeatherTypeRain},
        {"STORM", WeatherTypeStorm},
        {"SNOW", WeatherTypeSnow},
    }};
    for (const auto& [name, type] : entries) {
        if (value == name) {
            return type;
        }
    }
    return WeatherTypeNone;
}
}  // namespace

int WeatherController::weatherType_ = WeatherTypeNone;
float WeatherController::power_ = 0.0f;
float WeatherController::maxCount_ = 50.0f;
float WeatherController::time_ = 0.0f;
float WeatherController::stormFlashCooldown_ = 0.0f;
std::shared_ptr<sf::Shader> WeatherController::weatherShader_;
bool WeatherController::shaderFailed_ = false;
bool WeatherController::useParticles_ = false;
std::unique_ptr<sf::RenderTexture> WeatherController::weatherBuffer_;
std::optional<sf::Sprite> WeatherController::bufferSprite_;
std::vector<std::shared_ptr<Particle>> WeatherController::particles_;
std::weak_ptr<ParticleSystem> WeatherController::particleSystem_;
std::mt19937 WeatherController::random_{std::random_device{}()};

int coerceWeatherType(const RuntimeValue& value) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        return checkedWeatherType(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        return checkedWeatherType(
            static_cast<std::int64_t>(std::floor(*number)));
    }
    const std::string* string = value.getIf<std::string>();
    if (string == nullptr) {
        return WeatherTypeNone;
    }
    const std::string stripped = trim(*string);
    if (stripped.empty()) {
        return WeatherTypeNone;
    }
    char* end = nullptr;
    const double numeric = std::strtod(stripped.c_str(), &end);
    if (end != stripped.c_str() && *end == '\0' && std::isfinite(numeric) &&
        numeric == std::floor(numeric)) {
        return checkedWeatherType(static_cast<std::int64_t>(numeric));
    }
    return weatherTypeByName(stripped);
}

std::vector<std::string> getWeatherTypeDropBoxItems() {
    std::vector<std::string> result;
    result.reserve(4);
    for (const std::string_view name :
         {std::string_view("NONE"), std::string_view("RAIN"),
          std::string_view("STORM"), std::string_view("SNOW")}) {
        result.emplace_back(name);
    }
    return result;
}

void WeatherController::setWeather(const RuntimeValue& weatherType, float power,
                                   int maxCount) {
    clearWeather();
    weatherType_ = coerceWeatherType(weatherType);
    power_ = std::clamp(std::floor(power), 0.0f, 100.0f);
    maxCount_ = static_cast<float>(std::clamp(maxCount, 0, 100));
    if (weatherType_ == WeatherTypeNone || power_ <= 0.0f) {
        weatherType_ = WeatherTypeNone;
        return;
    }
    useParticles_ = shaderFailed_;
    if (!useParticles_) {
        ensureShader();
        if (weatherShader_ == nullptr) {
            useParticles_ = true;
        }
    }
    if (useParticles_ && !particleSystem_.expired()) {
        spawnParticles();
    }
}

void WeatherController::clearWeather() {
    weatherType_ = WeatherTypeNone;
    power_ = 0.0f;
    clearParticles();
}

void WeatherController::shutdown() noexcept {
    clearWeather();
    particleSystem_.reset();
    bufferSprite_.reset();
    weatherBuffer_.reset();
    weatherShader_.reset();
    shaderFailed_ = false;
    time_ = 0.0f;
    stormFlashCooldown_ = 0.0f;
}

void WeatherController::registerParticleSystem(
    const std::shared_ptr<ParticleSystem>& particleSystem) {
    if (particleSystem_.lock() == particleSystem) {
        return;
    }
    clearParticles();
    particleSystem_ = particleSystem;
    if (useParticles_ && weatherType_ != WeatherTypeNone && power_ > 0.0f) {
        spawnParticles();
    }
}

void WeatherController::update(float deltaTime) {
    if (weatherType_ == WeatherTypeNone || power_ <= 0.0f) {
        return;
    }
    time_ += deltaTime;
    if (weatherType_ != WeatherTypeStorm || useParticles_) {
        return;
    }
    stormFlashCooldown_ = std::max(0.0f, stormFlashCooldown_ - deltaTime);
    if (stormFlashCooldown_ <= 0.0f &&
        randomUnit() < 0.02f * (power_ / 100.0f)) {
        System::flashScreen(StormFlashColour, 0.08f + randomUnit() * 0.07f);
        stormFlashCooldown_ = 0.35f + randomUnit() * 0.65f;
    }
}

void WeatherController::drawShaderOverlay(Camera& camera) {
    if (useParticles_ || weatherType_ == WeatherTypeNone || power_ <= 0.0f ||
        weatherShader_ == nullptr) {
        return;
    }
    drawShaderOverlayTo(*camera.getRenderTexture());
}

void WeatherController::drawShaderOverlayTo(sf::RenderTexture& renderTexture) {
    const sf::Vector2u size = renderTexture.getSize();
    sf::RenderTexture& buffer = ensureBuffer(size);
    sf::Sprite& sprite = ensureBufferSprite();
    const sf::Texture& sourceTexture = renderTexture.getTexture();
    sprite.setTexture(sourceTexture, true);
    sprite.setPosition({0.0f, 0.0f});
    sprite.setScale({1.0f, 1.0f});
    weatherShader_->setUniform("screenTex", sourceTexture);
    weatherShader_->setUniform(
        "texSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
    weatherShader_->setUniform("time", time_);
    weatherShader_->setUniform("weatherType", static_cast<float>(weatherType_));
    weatherShader_->setUniform("power", power_ / 100.0f);
    weatherShader_->setUniform("maxScale", std::max(0.1f, maxCount_ / 50.0f));
    buffer.clear(sf::Color::Transparent);
    sf::RenderStates states = canvasRenderStates();
    states.shader = weatherShader_.get();
    buffer.draw(sprite, states);
    buffer.display();
    const sf::View savedView = renderTexture.getView();
    renderTexture.clear(sf::Color::Transparent);
    renderTexture.setView(renderTexture.getDefaultView());
    sprite.setTexture(buffer.getTexture(), true);
    renderTexture.draw(sprite, canvasRenderStates());
    renderTexture.setView(savedView);
    renderTexture.display();
}

int WeatherController::getWeatherType() {
    return weatherType_;
}

void WeatherController::ensureShader() {
    if (weatherShader_ != nullptr || shaderFailed_) {
        return;
    }
    try {
        weatherShader_ = ShaderManager::load("Global/Weather.frag");
    } catch (const std::exception&) {
        weatherShader_.reset();
        shaderFailed_ = true;
        warnOnce(
            "WeatherController.shader",
            "Weather shader failed to load; falling back to particle weather");
    }
}

sf::RenderTexture& WeatherController::ensureBuffer(const sf::Vector2u& size) {
    if (weatherBuffer_ == nullptr || weatherBuffer_->getSize() != size) {
        weatherBuffer_ = std::make_unique<sf::RenderTexture>(size);
        bufferSprite_.emplace(weatherBuffer_->getTexture());
    }
    return *weatherBuffer_;
}

sf::Sprite& WeatherController::ensureBufferSprite() {
    if (!bufferSprite_.has_value()) {
        bufferSprite_.emplace(weatherBuffer_->getTexture());
    }
    return *bufferSprite_;
}

void WeatherController::clearParticles() {
    for (const std::shared_ptr<Particle>& particle : particles_) {
        if (particle != nullptr && particle->getParent() != nullptr) {
            particle->destroy();
        }
    }
    particles_.clear();
}

void WeatherController::spawnParticles() {
    const std::shared_ptr<ParticleSystem> particleSystem =
        particleSystem_.lock();
    if (particleSystem == nullptr) {
        return;
    }
    clearParticles();
    const int count = particleCount();
    if (count <= 0) {
        return;
    }
    const float width = static_cast<float>(GameSize.x);
    const float height = static_cast<float>(GameSize.y);
    const std::function<void(float, float, ParticleBase*)> mover =
        buildMover(width, height);
    sf::Color colour;
    sf::Vector2f scale;
    sf::Angle rotation;
    if (weatherType_ == WeatherTypeSnow) {
        const std::uint8_t alpha = static_cast<std::uint8_t>(
            std::floor(140.0f + 80.0f * (power_ / 100.0f)));
        colour = {245, 248, 255, alpha};
        scale = {0.12f + 0.08f * randomUnit(), 0.12f + 0.08f * randomUnit()};
        rotation = sf::degrees(randomUnit() * 360.0f);
    } else {
        const std::uint8_t alpha = static_cast<std::uint8_t>(
            std::floor(120.0f + 90.0f * (power_ / 100.0f)));
        const float stretch = weatherType_ == WeatherTypeStorm ? 0.55f : 0.45f;
        colour = {180, 200, 235, alpha};
        scale = {0.05f + 0.03f * randomUnit(), stretch + 0.15f * randomUnit()};
        rotation = sf::degrees(-28.0f + randomUnit() * 16.0f);
    }
    particles_.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        ParticleInfo info;
        info.position = {randomUnit() * width,
                         -height * 0.2f + randomUnit() * height * 1.2f};
        info.color = colour;
        info.rotation = rotation;
        info.scale = scale;
        std::shared_ptr<Particle> particle = std::make_shared<Particle>(
            particleSystem, mover, randomUnit() * 2.0f,
            std::string(WeatherParticlePath), info);
        particleSystem->addParticle(particle);
        particles_.push_back(std::move(particle));
    }
}

int WeatherController::particleCount() {
    const float powerNormal = power_ / 100.0f;
    const float maxNormal = std::max(maxCount_, 1.0f) / 100.0f;
    float base = 12.0f + maxNormal * 188.0f;
    if (weatherType_ == WeatherTypeStorm) {
        base *= 1.35f;
    } else if (weatherType_ == WeatherTypeSnow) {
        base *= 0.85f;
    }
    return static_cast<int>(std::floor(base * powerNormal));
}

std::function<void(float, float, ParticleBase*)> WeatherController::buildMover(
    float width, float height) {
    if (weatherType_ == WeatherTypeSnow) {
        constexpr float wind = 18.0f;
        const float fall = 55.0f + 35.0f * (power_ / 100.0f);
        return [width, height, fall](float deltaTime, float,
                                     ParticleBase* particleBase) {
            Particle* particle = dynamic_cast<Particle*>(particleBase);
            if (particle == nullptr) {
                return;
            }
            particle->info.position.x += wind * deltaTime;
            particle->info.position.y += fall * deltaTime;
            if (particle->info.position.y > height + 16.0f) {
                particle->info.position.y = -24.0f + randomUnit() * 20.0f;
                particle->info.position.x = randomUnit() * width;
            }
            if (particle->info.position.x > width + 16.0f) {
                particle->info.position.x = -16.0f;
            } else if (particle->info.position.x < -16.0f) {
                particle->info.position.x = width + 16.0f;
            }
        };
    }
    const float wind = weatherType_ == WeatherTypeStorm ? -70.0f : -55.0f;
    float fall = weatherType_ == WeatherTypeStorm ? 360.0f : 280.0f;
    fall *= 0.75f + 0.5f * (power_ / 100.0f);
    return [width, height, wind, fall](float deltaTime, float,
                                       ParticleBase* particleBase) {
        Particle* particle = dynamic_cast<Particle*>(particleBase);
        if (particle == nullptr) {
            return;
        }
        particle->info.position.x += wind * deltaTime;
        particle->info.position.y += fall * deltaTime;
        if (particle->info.position.y > height + 24.0f) {
            particle->info.position.y = -48.0f + randomUnit() * 40.0f;
            particle->info.position.x = randomUnit() * width;
        }
    };
}

float WeatherController::randomUnit() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(random_);
}
