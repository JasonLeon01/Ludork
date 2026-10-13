#pragma once

#include <CoreMinimal.hpp>
#include <Graphics/MapFogSettings.hpp>
#include <Graphics/MapRenderer.hpp>

BIND_CLASS()
class FogController {
public:
    BIND_INIT()
    FogController();
    ~FogController();
    FogController(const FogController&) = delete;
    FogController& operator=(const FogController&) = delete;
    BIND_METHOD()
    void applyFromMapData(const MapFogSettings& settings);
    BIND_METHOD(metadata = false)
    void applyWorldFromMapData(const MapFogSettings& settings);
    BIND_METHOD(metadata = false)
    void setWorldRegionFog(const std::string& key, const sf::IntRect& cellRect,
                           const MapFogSettings& settings);
    BIND_METHOD(metadata = false)
    void removeWorldRegionFog(const std::string& key);
    BIND_METHOD()
    void clearFog();
    BIND_METHOD(Pure = true)
    MapFogSettings getSettings();
    BIND_METHOD(metadata = false)
    std::optional<MapFogSettings> getWorldRegionSettings(
        const std::string& key);
    void applyToScene(MapRenderer::Scene& scene);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
