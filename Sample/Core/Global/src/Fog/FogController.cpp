#include <Fog/FogController.hpp>

#include <Manager/AssetPath.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <System.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {
const RuntimeValue* findValue(const RuntimeValue::Map& map,
                              const std::string& key) {
    const auto iterator = map.find(key);
    return iterator == map.end() ? nullptr : &iterator->second;
}

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string stringValue(const RuntimeValue* value) {
    if (value == nullptr || value->isNil()) {
        return {};
    }
    if (const std::string* string = value->getIf<std::string>()) {
        return *string;
    }
    if (const std::int64_t* integer = value->getIf<std::int64_t>()) {
        return std::to_string(*integer);
    }
    if (const double* number = value->getIf<double>()) {
        return std::to_string(*number);
    }
    if (const bool* boolean = value->getIf<bool>()) {
        return *boolean ? "true" : "false";
    }
    return {};
}

double numberValue(const RuntimeValue* value, double fallback = 0.0) {
    if (value == nullptr) {
        return fallback;
    }
    if (const std::int64_t* integer = value->getIf<std::int64_t>()) {
        return static_cast<double>(*integer);
    }
    if (const double* number = value->getIf<double>()) {
        return *number;
    }
    const std::string* string = value->getIf<std::string>();
    if (string == nullptr) {
        return fallback;
    }
    char* end = nullptr;
    const double result = std::strtod(string->c_str(), &end);
    return end != string->c_str() && *end == '\0' && std::isfinite(result)
               ? result
               : fallback;
}

}  // namespace

std::string FogController::graphic_;
float FogController::power_ = 0.0f;
sf::Vector2f FogController::scroll_;
float FogController::distort_ = 0.0f;
sf::Vector2f FogController::offset_;
float FogController::time_ = 0.0f;
bool FogController::active_ = false;
std::shared_ptr<sf::Texture> FogController::fogTexture_;
std::optional<sf::Sprite> FogController::fogSprite_;
std::shared_ptr<sf::Shader> FogController::fogShader_;
bool FogController::shaderFailed_ = false;
std::unique_ptr<sf::RenderTexture> FogController::fogBuffer_;
std::optional<sf::Sprite> FogController::bufferSprite_;

void FogController::applyFromMapData(const RuntimeValue::Map& mapData) {
    clearFog();
    const std::string graphic = trim(stringValue(findValue(mapData, "fog")));
    const int power = static_cast<int>(
        std::floor(numberValue(findValue(mapData, "fogPower"))));
    if (graphic.empty() || power <= 0) {
        return;
    }
    graphic_ = graphic;
    power_ = static_cast<float>(std::clamp(power, 0, 100));
    scroll_ = {
        static_cast<float>(numberValue(findValue(mapData, "fogOx"))),
        static_cast<float>(numberValue(findValue(mapData, "fogOy"))),
    };
    distort_ = static_cast<float>(std::clamp(
        static_cast<int>(
            std::floor(numberValue(findValue(mapData, "fogDistort")))),
        0, 100));
    offset_ = {};
    time_ = 0.0f;
    if (!loadFogTexture()) {
        clearFog();
        return;
    }
    active_ = true;
    ensureShader();
    if (fogShader_ == nullptr) {
        ensureFallbackSprite();
    }
}

void FogController::clearFog() {
    active_ = false;
    graphic_.clear();
    power_ = 0.0f;
    scroll_ = {};
    distort_ = 0.0f;
    offset_ = {};
    time_ = 0.0f;
    fogTexture_.reset();
    fogSprite_.reset();
}

void FogController::shutdown() noexcept {
    clearFog();
    bufferSprite_.reset();
    fogBuffer_.reset();
    fogShader_.reset();
    shaderFailed_ = false;
}

void FogController::update(float deltaTime) {
    if (!active_ || power_ <= 0.0f) {
        return;
    }
    time_ += deltaTime;
    offset_ += scroll_ * deltaTime;
    if (fogShader_ == nullptr && fogSprite_.has_value() &&
        fogTexture_ != nullptr) {
        updateFallbackSprite();
    }
}

void FogController::drawOverlay() {
    if (!active_ || power_ <= 0.0f || fogTexture_ == nullptr) {
        return;
    }
    sf::RenderTexture* canvas = System::getCanvas();
    if (canvas == nullptr) {
        return;
    }
    if (fogShader_ == nullptr) {
        drawFallbackOverlay(*canvas);
    } else {
        drawShaderOverlay(*canvas);
    }
}

bool FogController::loadFogTexture() {
    try {
        fogTexture_ = TextureManager::load(
            ludork::global::manager::textureAssetFile("Fogs", graphic_), false,
            std::nullopt, true);
        if (fogTexture_ == nullptr) {
            return false;
        }
        fogTexture_->setRepeated(true);
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Failed to load fog texture '" << graphic_
                  << "': " << error.what() << '\n';
        return false;
    }
}

void FogController::ensureShader() {
    if (fogShader_ != nullptr || shaderFailed_) {
        return;
    }
    try {
        fogShader_ = ShaderManager::load("Global/Fog.frag");
    } catch (const std::exception&) {
        fogShader_.reset();
        shaderFailed_ = true;
        warnOnce("FogController.shader",
                 "Fog shader failed to load; falling back to sprite fog");
    }
}

void FogController::ensureFallbackSprite() {
    if (fogTexture_ == nullptr) {
        return;
    }
    const sf::Vector2u gameSize = System::getGameSize();
    if (!fogSprite_.has_value()) {
        fogSprite_.emplace(*fogTexture_);
    } else {
        fogSprite_->setTexture(*fogTexture_, true);
    }
    fogSprite_->setTextureRect(sf::IntRect(
        {0, 0}, {static_cast<int>(gameSize.x), static_cast<int>(gameSize.y)}));
    updateFallbackSprite();
}

void FogController::updateFallbackSprite() {
    if (!fogSprite_.has_value() || fogTexture_ == nullptr) {
        return;
    }
    const sf::Vector2u textureSize = fogTexture_->getSize();
    const float width = static_cast<float>(std::max(1u, textureSize.x));
    const float height = static_cast<float>(std::max(1u, textureSize.y));
    float offsetX = std::fmod(offset_.x, width);
    float offsetY = std::fmod(offset_.y, height);
    if (offsetX < 0.0f) {
        offsetX += width;
    }
    if (offsetY < 0.0f) {
        offsetY += height;
    }
    fogSprite_->setPosition({-offsetX, -offsetY});
    const std::uint8_t alpha = static_cast<std::uint8_t>(
        std::clamp(std::floor(255.0f * power_ / 100.0f), 0.0f, 255.0f));
    fogSprite_->setColor({255, 255, 255, alpha});
}

void FogController::drawFallbackOverlay(sf::RenderTexture& canvas) {
    if (!fogSprite_.has_value()) {
        ensureFallbackSprite();
    }
    if (fogSprite_.has_value()) {
        canvas.draw(*fogSprite_, canvasRenderStates());
    }
}

void FogController::drawShaderOverlay(sf::RenderTexture& canvas) {
    if (fogShader_ == nullptr || fogTexture_ == nullptr) {
        drawFallbackOverlay(canvas);
        return;
    }
    const sf::Vector2u size = canvas.getSize();
    sf::RenderTexture& buffer = ensureBuffer(size);
    sf::Sprite& sprite = ensureBufferSprite();
    const sf::Texture& sourceTexture = canvas.getTexture();
    sprite.setTexture(sourceTexture, true);
    sprite.setPosition({0.0f, 0.0f});
    sprite.setScale({1.0f, 1.0f});
    const sf::Vector2u textureSize = fogTexture_->getSize();
    const sf::Vector2f fogScroll{
        offset_.x / static_cast<float>(std::max(1u, textureSize.x)),
        offset_.y / static_cast<float>(std::max(1u, textureSize.y)),
    };
    fogShader_->setUniform("screenTex", sourceTexture);
    fogShader_->setUniform("fogTex", *fogTexture_);
    fogShader_->setUniform("texSize", sf::Vector2f{static_cast<float>(size.x),
                                                   static_cast<float>(size.y)});
    fogShader_->setUniform("fogScroll", fogScroll);
    fogShader_->setUniform("power", power_ / 100.0f);
    fogShader_->setUniform("distort", distort_ / 100.0f);
    fogShader_->setUniform("time", time_);
    buffer.clear(sf::Color::Transparent);
    sf::RenderStates states = canvasRenderStates();
    states.shader = fogShader_.get();
    buffer.draw(sprite, states);
    buffer.display();
    const sf::View savedView = canvas.getView();
    canvas.clear(sf::Color::Transparent);
    canvas.setView(canvas.getDefaultView());
    sprite.setTexture(buffer.getTexture(), true);
    canvas.draw(sprite, canvasRenderStates());
    canvas.setView(savedView);
    canvas.display();
}

sf::RenderTexture& FogController::ensureBuffer(const sf::Vector2u& size) {
    if (fogBuffer_ == nullptr || fogBuffer_->getSize() != size) {
        fogBuffer_ = std::make_unique<sf::RenderTexture>(size);
        bufferSprite_.emplace(fogBuffer_->getTexture());
    }
    return *fogBuffer_;
}

sf::Sprite& FogController::ensureBufferSprite() {
    if (!bufferSprite_.has_value()) {
        bufferSprite_.emplace(fogBuffer_->getTexture());
    }
    return *bufferSprite_;
}
