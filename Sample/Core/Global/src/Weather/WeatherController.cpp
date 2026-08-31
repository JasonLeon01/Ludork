#include <Weather/WeatherController.hpp>

#include <Camera.hpp>
#include <Manager/ShaderManager.hpp>
#include <System.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
const sf::Color StormFlashColour{210, 220, 255, 120};

WeatherType checkedWeatherType(WeatherType value) {
    switch (value) {
        case WeatherType::NONE:
        case WeatherType::RAIN:
        case WeatherType::STORM:
        case WeatherType::SNOW:
            return value;
    }
    throw std::invalid_argument("unsupported weather type");
}
}  // namespace

WeatherType WeatherController::weatherType_ = WeatherType::NONE;
float WeatherController::power_ = 0.0f;
float WeatherController::maxCount_ = 50.0f;
float WeatherController::time_ = 0.0f;
float WeatherController::stormFlashCooldown_ = 0.0f;
std::shared_ptr<sf::Shader> WeatherController::weatherShader_;
std::unique_ptr<sf::RenderTexture> WeatherController::weatherBuffer_;
std::optional<sf::Sprite> WeatherController::bufferSprite_;
std::mt19937 WeatherController::random_{std::random_device{}()};

void WeatherController::setWeather(WeatherType weatherType, float power,
                                   int maxCount) {
    clearWeather();
    weatherType_ = checkedWeatherType(weatherType);
    power_ = std::clamp(std::floor(power), 0.0f, 100.0f);
    maxCount_ = static_cast<float>(std::clamp(maxCount, 0, 100));
    if (weatherType_ == WeatherType::NONE || power_ <= 0.0f) {
        weatherType_ = WeatherType::NONE;
        return;
    }
    ensureShader();
}

void WeatherController::clearWeather() {
    weatherType_ = WeatherType::NONE;
    power_ = 0.0f;
}

void WeatherController::shutdown() noexcept {
    clearWeather();
    bufferSprite_.reset();
    weatherBuffer_.reset();
    weatherShader_.reset();
    time_ = 0.0f;
    stormFlashCooldown_ = 0.0f;
}

void WeatherController::update(float deltaTime) {
    if (weatherType_ == WeatherType::NONE || power_ <= 0.0f) {
        return;
    }
    time_ += deltaTime;
    if (weatherType_ != WeatherType::STORM) {
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
    if (weatherType_ == WeatherType::NONE || power_ <= 0.0f) {
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
    weatherShader_->setUniform(
        "weatherType", static_cast<float>(static_cast<int>(weatherType_)));
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

WeatherType WeatherController::getWeatherType() {
    return weatherType_;
}

void WeatherController::ensureShader() {
    if (weatherShader_ != nullptr) {
        return;
    }
    weatherShader_ = ShaderManager::load("Global/Weather.frag");
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

float WeatherController::randomUnit() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(random_);
}
