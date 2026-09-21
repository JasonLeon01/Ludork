#include <Graphics.hpp>
#include <Panorama/PanoramaController.hpp>

#include "PanoramaControllerImpl.hpp"

#include <Camera.hpp>
#include <EngineState.hpp>
#include <GameMapBase.hpp>
#include <Manager/TextureManager.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

std::string trim(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

float scrollFactor(float cameraPosition, float mapLength, float viewLength) {
    const float travel = mapLength - viewLength;
    if (travel <= 0.0f) {
        return 0.5f;
    }
    return std::clamp(cameraPosition / travel, 0.0f, 1.0f);
}

sf::Color ambientTint(const sf::Color& ambient) {
    const unsigned scale = ambient.a;
    return {static_cast<std::uint8_t>(static_cast<unsigned>(ambient.r) * scale /
                                      255u),
            static_cast<std::uint8_t>(static_cast<unsigned>(ambient.g) * scale /
                                      255u),
            static_cast<std::uint8_t>(static_cast<unsigned>(ambient.b) * scale /
                                      255u),
            255};
}

auto& graphic_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().graphic;
auto& active_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().active;
auto& texture_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().texture;
auto& sprite_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().sprite;

}  // namespace

void PanoramaController::applyFromMapData(const MapPanoramaSettings& mapData) {
    clear();
    const std::string graphic = trim(mapData.panorama);
    if (graphic.empty()) {
        return;
    }
    graphic_ = graphic;
    texture_ = TextureManager::load(graphic, false, std::nullopt, true);
    if (texture_ == nullptr) {
        throw std::runtime_error("Failed to load panorama texture: " + graphic);
    }
    sprite_.emplace(*texture_);
    active_ = true;
}

void PanoramaController::applyWorldFromMapData(
    const MapPanoramaSettings& mapData) {
    applyFromMapData(mapData);
}

void PanoramaController::clear() {
    active_ = false;
    graphic_.clear();
    texture_.reset();
    sprite_.reset();
}

bool PanoramaController::isActive() {
    return active_ && texture_ != nullptr;
}

void PanoramaController::drawUnderlay(Camera& camera,
                                      const sf::Color& ambientLight) {
    if (!isActive()) {
        return;
    }
    sf::RenderTexture* canvas = Graphics::getCanvas();
    const std::optional<sf::Vector2f> viewPosition = camera.getViewPosition();
    const std::optional<sf::Vector2f> viewSize = camera.getViewSize();
    const std::shared_ptr<GameMapBase> map = camera.getMap();
    if (canvas == nullptr || !viewPosition.has_value() ||
        !viewSize.has_value() || map == nullptr || texture_ == nullptr) {
        return;
    }
    const sf::Vector2u textureSize = texture_->getSize();
    const float canvasWidth = viewSize->x;
    const float canvasHeight = viewSize->y;
    if (textureSize.x == 0 || textureSize.y == 0 || canvasWidth <= 0.0f ||
        canvasHeight <= 0.0f) {
        return;
    }
    const sf::Vector2u mapSize = map->getSize();
    const float mapWidth =
        static_cast<float>(mapSize.x * EngineState::CellSize);
    const float mapHeight =
        static_cast<float>(mapSize.y * EngineState::CellSize);
    const float scale =
        std::max(canvasWidth / static_cast<float>(textureSize.x),
                 canvasHeight / static_cast<float>(textureSize.y));
    const float scaledWidth = static_cast<float>(textureSize.x) * scale;
    const float scaledHeight = static_cast<float>(textureSize.y) * scale;
    const float overflowX = std::max(0.0f, scaledWidth - canvasWidth);
    const float overflowY = std::max(0.0f, scaledHeight - canvasHeight);
    const float cropX =
        scrollFactor(viewPosition->x, mapWidth, canvasWidth) * overflowX;
    const float cropY =
        scrollFactor(viewPosition->y, mapHeight, canvasHeight) * overflowY;
    if (!sprite_.has_value()) {
        sprite_.emplace(*texture_);
    } else {
        sprite_->setTexture(*texture_, true);
    }
    sprite_->setScale({scale, scale});
    sprite_->setPosition({-cropX, -cropY});
    sprite_->setColor(ambientTint(ambientLight));
    canvas->draw(*sprite_);
}

void PanoramaController::shutdown() noexcept {
    clear();
}
