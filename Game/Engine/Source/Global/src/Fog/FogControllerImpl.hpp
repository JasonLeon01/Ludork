#pragma once

#include <Fog/FogController.hpp>

class FogController::Impl {
public:
    struct Region {
        std::string key;
        sf::IntRect cellRect;
        MapFogSettings settings;
    };
    MapFogSettings settings;
    std::vector<Region> regions;
};
