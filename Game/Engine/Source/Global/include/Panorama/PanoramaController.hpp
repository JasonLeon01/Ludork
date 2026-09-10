#pragma once

#include <CoreMinimal.hpp>
#include <Panorama/MapPanoramaSettings.hpp>

class Camera;

BIND_CLASS()
class PanoramaController {
public:
    BIND_METHOD()
    static void applyFromMapData(const MapPanoramaSettings& mapData);

    BIND_METHOD(metadata = false)
    static void applyWorldFromMapData(const MapPanoramaSettings& mapData);

    BIND_METHOD()
    static void clear();

    BIND_METHOD(Pure = true)
    static bool isActive();

    BIND_METHOD(metadata = false)
    static void drawUnderlay(Camera& camera, const sf::Color& ambientLight);

    static void shutdown() noexcept;
};
