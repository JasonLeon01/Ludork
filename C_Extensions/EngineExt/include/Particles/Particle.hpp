#pragma once

#include <SFML/Graphics.hpp>
#include <string>

#include "Particles/ParticleBase.hpp"

// BIND_CLASS
// Sprite-based particle with resource path and transform info.
class Particle : public ParticleBase {
public:
    Particle() = delete;

    // BIND_INIT
    Particle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction, float countTime,
             const std::string& inResourcePath, ParticleInfo inInfo);

    // BIND_METHOD
    virtual void onTick(float deltaTime) override;

    // BIND_METHOD
    virtual void onLateTick(float deltaTime) override;

    // BIND_METHOD
    virtual void onFixedTick(float fixedDelta) override;

    // BIND_METHOD
    virtual void destroy();

    // BIND_PROPERTY
    std::string resourcePath;

    // BIND_PROPERTY
    ParticleInfo info;

private:
    void checkUpdate();
    sf::Vector2f lastPosition_;
    sf::Angle lastRotation_;
    sf::Vector2f lastScale_;
    sf::Color lastColor_;
};
