#include <Fog/FogController.hpp>
#include <Gameplay/Tilemap/Tilemap.hpp>
#include "FogControllerImpl.hpp"

#include <algorithm>

FogController::FogController() : impl_(std::make_unique<Impl>()) {}
FogController::~FogController() = default;

void FogController::applyFromMapData(const MapFogSettings& settings) {
    impl_->settings = settings;
}

void FogController::applyWorldFromMapData(const MapFogSettings& settings) {
    clearFog();
    applyFromMapData(settings);
}

void FogController::setWorldRegionFog(const std::string& key,
                                      const sf::IntRect& cellRect,
                                      const MapFogSettings& settings) {
    auto& regions = impl_->regions;
    const auto found = std::find_if(regions.begin(), regions.end(),
                                    [&key](const auto& region) {
                                        return region.key == key;
                                    });
    if (found == regions.end()) {
        regions.push_back({key, cellRect, settings});
    } else {
        *found = {key, cellRect, settings};
    }
}

void FogController::removeWorldRegionFog(const std::string& key) {
    auto& regions = impl_->regions;
    std::erase_if(regions, [&key](const auto& region) {
        return region.key == key;
    });
}

void FogController::clearFog() {
    *impl_ = {};
}

MapFogSettings FogController::getSettings() {
    return impl_->settings;
}

std::optional<MapFogSettings> FogController::getWorldRegionSettings(
    const std::string& key) {
    const auto& regions = impl_->regions;
    const auto found = std::find_if(regions.begin(), regions.end(),
                                    [&key](const auto& region) {
                                        return region.key == key;
                                    });
    return found == regions.end()
               ? std::nullopt
               : std::optional<MapFogSettings>(found->settings);
}

void FogController::applyToScene(MapRenderer::Scene& scene) {
    const auto& state = *impl_;
    scene.fog = state.settings;
    for (MapRenderer::Region& region : scene.regions) {
        region.fog.reset();
        if (!region.tilemap) {
            continue;
        }
        const sf::IntRect rect(region.cellOrigin,
                               sf::Vector2i(region.tilemap->getSize()));
        const auto found =
            std::find_if(state.regions.begin(), state.regions.end(),
                         [&rect](const auto& entry) {
                             return entry.cellRect == rect;
                         });
        if (found != state.regions.end()) {
            region.fog = found->settings;
        }
    }
}
