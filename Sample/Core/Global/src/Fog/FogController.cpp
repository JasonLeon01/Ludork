#include <Fog/FogController.hpp>

#include <Camera.hpp>
#include <Manager/AssetPath.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <System.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {
using ludork::engine::runtime_value_reader::findValue;
using ludork::engine::runtime_value_reader::requireFloat;
using ludork::engine::runtime_value_reader::requireString;

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

float optionalFloat(const RuntimeValue::Map& mapData, const std::string& name) {
    const RuntimeValue* value = findValue(mapData, name);
    return value == nullptr || value->isNil()
               ? 0.0f
               : requireFloat(*value, "mapData." + name);
}

struct WorldFogLayer {
    std::string graphic;
    float power = 0.0f;
    sf::Vector2f scroll;
    float distort = 0.0f;
    sf::IntRect cellRect;
    std::shared_ptr<sf::Texture> texture;
    std::optional<sf::Sprite> sprite;
};

struct WorldFogState {
    std::optional<WorldFogLayer> base;
    std::unordered_map<std::string, WorldFogLayer> regions;
    float time = 0.0f;
};

std::optional<WorldFogState> worldFogState;

std::optional<WorldFogLayer> makeWorldFogLayer(std::string graphic, float power,
                                               const sf::Vector2f& scroll,
                                               float distort,
                                               const sf::IntRect& cellRect) {
    if (!std::isfinite(power) || !std::isfinite(scroll.x) ||
        !std::isfinite(scroll.y) || !std::isfinite(distort)) {
        throw std::invalid_argument("world fog values must be finite");
    }
    graphic = trim(std::move(graphic));
    power = std::clamp(std::floor(power), 0.0f, 100.0f);
    if (graphic.empty() || power <= 0.0f) {
        return std::nullopt;
    }
    WorldFogLayer layer;
    layer.graphic = graphic;
    layer.power = power;
    layer.scroll = scroll;
    layer.distort = std::clamp(std::floor(distort), 0.0f, 100.0f);
    layer.cellRect = cellRect;
    try {
        layer.texture = TextureManager::load(
            ludork::global::manager::textureAssetFile("Fogs", graphic), false,
            std::nullopt, true);
        if (layer.texture == nullptr) {
            return std::nullopt;
        }
        layer.texture->setRepeated(true);
        layer.sprite.emplace(*layer.texture);
    } catch (const std::exception& error) {
        std::cerr << "Failed to load fog texture '" << graphic
                  << "': " << error.what() << '\n';
        return std::nullopt;
    }
    return layer;
}

std::optional<WorldFogLayer> makeWorldFogLayer(const RuntimeValue::Map& mapData,
                                               const sf::IntRect& cellRect) {
    const RuntimeValue* fogValue = findValue(mapData, "fog");
    return makeWorldFogLayer(
        fogValue == nullptr || fogValue->isNil()
            ? std::string{}
            : requireString(*fogValue, "mapData.fog"),
        optionalFloat(mapData, "fogPower"),
        {optionalFloat(mapData, "fogOx"), optionalFloat(mapData, "fogOy")},
        optionalFloat(mapData, "fogDistort"), cellRect);
}

std::optional<sf::FloatRect> worldFogRect(const WorldFogLayer& layer,
                                          const sf::FloatRect& viewBounds,
                                          bool base) {
    if (base) {
        return viewBounds;
    }
    const sf::FloatRect worldRect(
        sf::Vector2f(static_cast<float>(layer.cellRect.position.x * CellSize),
                     static_cast<float>(layer.cellRect.position.y * CellSize)),
        sf::Vector2f(static_cast<float>(layer.cellRect.size.x * CellSize),
                     static_cast<float>(layer.cellRect.size.y * CellSize)));
    const float left = std::max(worldRect.position.x, viewBounds.position.x);
    const float top = std::max(worldRect.position.y, viewBounds.position.y);
    const float right = std::min(worldRect.position.x + worldRect.size.x,
                                 viewBounds.position.x + viewBounds.size.x);
    const float bottom = std::min(worldRect.position.y + worldRect.size.y,
                                  viewBounds.position.y + viewBounds.size.y);
    if (left >= right || top >= bottom || viewBounds.size.x <= 0.0f ||
        viewBounds.size.y <= 0.0f) {
        return std::nullopt;
    }
    return worldRect;
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
    const RuntimeValue* fogValue = findValue(mapData, "fog");
    const std::string graphic =
        trim(fogValue == nullptr || fogValue->isNil()
                 ? std::string{}
                 : requireString(*fogValue, "mapData.fog"));
    const float power = std::clamp(
        std::floor(optionalFloat(mapData, "fogPower")), 0.0f, 100.0f);
    const sf::Vector2f scroll{optionalFloat(mapData, "fogOx"),
                              optionalFloat(mapData, "fogOy")};
    const float distort = std::clamp(
        std::floor(optionalFloat(mapData, "fogDistort")), 0.0f, 100.0f);
    if (graphic.empty() || power <= 0.0f) {
        return;
    }
    graphic_ = graphic;
    power_ = power;
    scroll_ = scroll;
    distort_ = distort;
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

void FogController::applyWorldFromMapData(const RuntimeValue::Map& mapData) {
    clearFog();
    worldFogState.emplace();
    worldFogState->base = makeWorldFogLayer(mapData, {});
    ensureShader();
}

void FogController::setWorldRegionFog(const std::string& key,
                                      const sf::IntRect& cellRect,
                                      const std::string& graphic, float power,
                                      float scrollX, float scrollY,
                                      float distort) {
    if (!worldFogState.has_value()) {
        return;
    }
    std::optional<WorldFogLayer> layer = makeWorldFogLayer(
        graphic, power, {scrollX, scrollY}, distort, cellRect);
    if (layer.has_value()) {
        worldFogState->regions.insert_or_assign(key, std::move(*layer));
    } else {
        worldFogState->regions.erase(key);
    }
    ensureShader();
}

void FogController::removeWorldRegionFog(const std::string& key) {
    if (!worldFogState.has_value()) {
        return;
    }
    worldFogState->regions.erase(key);
}

void FogController::clearFog() {
    worldFogState.reset();
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
    if (worldFogState.has_value()) {
        worldFogState->time += deltaTime;
        return;
    }
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
    if (worldFogState.has_value()) {
        return;
    }
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

void FogController::drawWorldOverlay(Camera& camera) {
    if (!worldFogState.has_value()) {
        return;
    }
    const std::optional<sf::FloatRect> viewValue = camera.getViewport();
    const std::shared_ptr<sf::RenderTexture> target = camera.getRenderTexture();
    if (!viewValue.has_value() || target == nullptr) {
        return;
    }
    const sf::Vector2u targetSize = target->getSize();
    if (targetSize.x == 0 || targetSize.y == 0) {
        return;
    }
    const sf::View targetView = target->getView();
    const sf::Vector2f topLeft = target->mapPixelToCoords({0, 0}, targetView);
    const sf::Vector2f topRight = target->mapPixelToCoords(
        {static_cast<int>(targetSize.x), 0}, targetView);
    const sf::Vector2f bottomLeft = target->mapPixelToCoords(
        {0, static_cast<int>(targetSize.y)}, targetView);
    const sf::Vector2f bottomRight = target->mapPixelToCoords(
        {static_cast<int>(targetSize.x), static_cast<int>(targetSize.y)},
        targetView);
    const float minimumX =
        std::min({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
    const float minimumY =
        std::min({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
    const float maximumX =
        std::max({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
    const float maximumY =
        std::max({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
    const sf::FloatRect viewBounds({minimumX, minimumY},
                                   {maximumX - minimumX, maximumY - minimumY});
    const sf::Vector2f worldAxisX = topRight - topLeft;
    const sf::Vector2f worldAxisY = bottomLeft - topLeft;
    const float worldTime = worldFogState->time;
    const auto drawLayer = [&](WorldFogLayer& layer, bool base) {
        const std::optional<sf::FloatRect> layerRect =
            worldFogRect(layer, viewBounds, base);
        if (!layerRect.has_value() || layer.texture == nullptr) {
            return;
        }
        target->display();
        if (fogShader_ == nullptr) {
            if (!layer.sprite.has_value()) {
                layer.sprite.emplace(*layer.texture);
            }
            const sf::FloatRect worldRect = *layerRect;
            layer.sprite->setPosition(worldRect.position);
            const sf::Vector2f offset = layer.scroll * worldTime;
            const sf::Vector2f samplePosition = worldRect.position + offset;
            layer.sprite->setTextureRect(
                sf::IntRect(sf::Vector2i(static_cast<int>(samplePosition.x),
                                         static_cast<int>(samplePosition.y)),
                            sf::Vector2i(static_cast<int>(worldRect.size.x),
                                         static_cast<int>(worldRect.size.y))));
            layer.sprite->setColor(sf::Color(
                255, 255, 255,
                static_cast<std::uint8_t>(std::clamp(
                    std::floor(255.0f * layer.power / 100.0f), 0.0f, 255.0f))));
            target->draw(*layer.sprite, canvasRenderStates());
            target->display();
            return;
        }
        const sf::Vector2u size = target->getSize();
        sf::RenderTexture& buffer = ensureBuffer(size);
        sf::Sprite& sprite = ensureBufferSprite();
        const sf::Texture& sourceTexture = target->getTexture();
        sprite.setTexture(sourceTexture, true);
        sprite.setTextureRect(
            {{0, 0}, {static_cast<int>(size.x), static_cast<int>(size.y)}});
        sprite.setPosition({0.0f, 0.0f});
        sprite.setScale({1.0f, 1.0f});
        const sf::Vector2u fogSize = layer.texture->getSize();
        fogShader_->setUniform("screenTex", sourceTexture);
        fogShader_->setUniform("fogTex", *layer.texture);
        fogShader_->setUniform(
            "fogScroll",
            sf::Vector2f(layer.scroll.x * worldTime /
                             static_cast<float>(std::max(1u, fogSize.x)),
                         layer.scroll.y * worldTime /
                             static_cast<float>(std::max(1u, fogSize.y))));
        fogShader_->setUniform("power", layer.power / 100.0f);
        fogShader_->setUniform("distort", layer.distort / 100.0f);
        fogShader_->setUniform("time", worldTime);
        fogShader_->setUniform("worldMode", 1.0f);
        fogShader_->setUniform("worldOrigin", topLeft);
        fogShader_->setUniform("worldAxisX", worldAxisX);
        fogShader_->setUniform("worldAxisY", worldAxisY);
        fogShader_->setUniform(
            "fogTextureSize",
            sf::Vector2f(static_cast<float>(std::max(1u, fogSize.x)),
                         static_cast<float>(std::max(1u, fogSize.y))));
        fogShader_->setUniform("clipMin", layerRect->position);
        fogShader_->setUniform("clipMax",
                               layerRect->position + layerRect->size);
        buffer.setView(buffer.getDefaultView());
        buffer.clear(sf::Color::Transparent);
        sf::RenderStates states = canvasRenderStates();
        states.shader = fogShader_.get();
        buffer.draw(sprite, states);
        buffer.display();
        const sf::View savedView = target->getView();
        target->setView(target->getDefaultView());
        target->clear(sf::Color::Transparent);
        sprite.setTexture(buffer.getTexture(), true);
        target->draw(sprite, canvasRenderStates());
        target->setView(savedView);
        target->display();
    };
    if (worldFogState->base.has_value()) {
        drawLayer(*worldFogState->base, true);
    }
    for (auto& [_, layer] : worldFogState->regions) {
        drawLayer(layer, false);
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
    fogShader_->setUniform("fogScroll", fogScroll);
    fogShader_->setUniform("power", power_ / 100.0f);
    fogShader_->setUniform("distort", distort_ / 100.0f);
    fogShader_->setUniform("time", time_);
    fogShader_->setUniform("worldMode", 0.0f);
    buffer.clear(sf::Color::Transparent);
    sf::RenderStates states = canvasRenderStates();
    states.shader = fogShader_.get();
    buffer.draw(sprite, states);
    buffer.display();
    const sf::View savedView = canvas.getView();
    const sf::IntRect viewport = canvas.getViewport(savedView);
    const sf::Vector2f viewSize = savedView.getSize();
    canvas.clear(sf::Color::Transparent);
    sprite.setTexture(buffer.getTexture(), true);
    sprite.setTextureRect(viewport);
    sprite.setPosition({0.0f, 0.0f});
    sprite.setScale({viewSize.x / static_cast<float>(viewport.size.x),
                     viewSize.y / static_cast<float>(viewport.size.y)});
    canvas.setView(savedView);
    canvas.draw(sprite, canvasRenderStates());
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
