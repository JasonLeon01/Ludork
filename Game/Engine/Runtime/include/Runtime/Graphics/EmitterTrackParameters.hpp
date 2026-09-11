#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>
#include <Runtime/Graphics/EmitterBurst.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>
#include <array>
#include <string>
#include <vector>

namespace ludork::runtime::graphics {

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LUDORK_RUNTIME_API EmitterTrackParameters {
    BIND_PROPERTY()
    std::string name = "Track";

    BIND_PROPERTY()
    std::string texture;

    BIND_PROPERTY()
    bool enabled = true;

    BIND_PROPERTY()
    bool resident = false;

    BIND_PROPERTY()
    bool loop = true;

    BIND_PROPERTY()
    bool prewarm = false;

    BIND_PROPERTY()
    bool world = false;

    BIND_PROPERTY()
    bool additive = false;

    BIND_PROPERTY()
    int scaleMode = 0;

    BIND_PROPERTY()
    int capacity = 1024;

    BIND_PROPERTY()
    int count = 32;

    BIND_PROPERTY()
    float delay = 0;

    BIND_PROPERTY()
    float duration = 2;

    BIND_PROPERTY()
    float rate = 30;

    BIND_PROPERTY()
    float distanceRate = 0;

    BIND_PROPERTY()
    std::vector<EmitterBurst> bursts;

    BIND_PROPERTY()
    int shape = 0;

    BIND_PROPERTY()
    sf::Vector2f extent{32, 32};

    BIND_PROPERTY()
    float radius = 16;

    BIND_PROPERTY()
    float innerRadius = 8;

    BIND_PROPERTY()
    float direction = -90;

    BIND_PROPERTY()
    float spread = 30;

    BIND_PROPERTY()
    sf::Vector2f lifetime{1, 2};

    BIND_PROPERTY()
    sf::Vector2f speed{20, 40};

    BIND_PROPERTY()
    sf::Vector2f sizeMin{8, 8};

    BIND_PROPERTY()
    sf::Vector2f sizeMax{16, 16};

    BIND_PROPERTY()
    sf::Vector2f rotation{0, 360};

    BIND_PROPERTY()
    sf::Vector2f angularVelocity{0, 0};

    BIND_PROPERTY()
    std::array<float, 4> colourMin{1, 1, 1, 1};

    BIND_PROPERTY()
    std::array<float, 4> colourMax{1, 1, 1, 1};

    BIND_PROPERTY()
    sf::Vector2f gravity{0, 0};

    BIND_PROPERTY()
    float radialAcceleration = 0;

    BIND_PROPERTY()
    float tangentialAcceleration = 0;

    BIND_PROPERTY()
    float damping = 0;

    BIND_PROPERTY()
    sf::IntRect textureRect;

    BIND_PROPERTY()
    int columns = 1;

    BIND_PROPERTY()
    int rows = 1;

    BIND_PROPERTY()
    int frameCount = 1;

    BIND_PROPERTY()
    float frameRate = 0;

    BIND_PROPERTY()
    bool randomStartFrame = false;

    BIND_PROPERTY()
    bool frameLoop = true;

    BIND_PROPERTY()
    sf::Vector2f offset;

    BIND_PROPERTY()
    float rotationOffset = 0;

    BIND_PROPERTY()
    sf::Vector2f scale{1, 1};
};

}  // namespace ludork::runtime::graphics
