#include <StringUtils.hpp>
#include <Panorama/PanoramaController.hpp>

#include "PanoramaControllerImpl.hpp"

#include <Manager/TextureManager.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

auto& graphic_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().graphic;
auto& active_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().active;
auto& texture_ =
    ludork::global::panorama_controller_impl::panoramaControllerImpl().texture;

}  // namespace

void PanoramaController::applyFromMapData(const MapPanoramaSettings& mapData) {
    clear();
    const std::string graphic =
        ludork::standard::trimCharacters(mapData.panorama);
    if (graphic.empty()) {
        return;
    }
    graphic_ = graphic;
    texture_ = TextureManager::load(graphic, false, std::nullopt, true);
    if (texture_ == nullptr) {
        throw std::runtime_error("Failed to load panorama texture: " + graphic);
    }
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
}

bool PanoramaController::isActive() {
    return active_ && texture_ != nullptr;
}

std::shared_ptr<sf::Texture> PanoramaController::getTexture() {
    return isActive() ? texture_ : nullptr;
}

std::string PanoramaController::getPath() {
    return graphic_;
}

void PanoramaController::shutdown() noexcept {
    clear();
}
