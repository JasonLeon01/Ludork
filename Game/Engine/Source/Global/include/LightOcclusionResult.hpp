#pragma once

#include <CoreMinimal.hpp>

class Actor;

BIND_CLASS(copyable = true, metadata = false)
struct LightOcclusionResult {
    BIND_PROPERTY(metadata = false)
    bool hasStaticTransmissionLoss = false;

    BIND_PROPERTY(metadata = false)
    std::vector<std::shared_ptr<Actor>> occluders;

    BIND_PROPERTY(metadata = false)
    std::optional<sf::FloatRect> maskRect;

    BIND_PROPERTY(metadata = false)
    std::shared_ptr<sf::Texture> dynamicOccupancy;

    BIND_PROPERTY(metadata = false)
    sf::Vector2f dynamicOccupancyOrigin;

    BIND_PROPERTY(metadata = false)
    sf::Vector2f dynamicOccupancySize;
};
