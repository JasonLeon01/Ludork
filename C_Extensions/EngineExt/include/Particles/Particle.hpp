#pragma once

#include <SFML/Graphics.hpp>
#include <string>

#include "Particles/ParticleBase.hpp"

class Particle : public ParticleBase {
public:
    Particle() = delete;
    Particle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction, float countTime,
             const std::string& inResourcePath, ParticleInfo inInfo);
    virtual void onTick(float deltaTime) override;
    virtual void onLateTick(float deltaTime) override;
    virtual void onFixedTick(float fixedDelta) override;
    virtual void destroy();
    std::string resourcePath;
    ParticleInfo info;

private:
    void checkUpdate();
    sf::Vector2f lastPosition_;
    sf::Angle lastRotation_;
    sf::Vector2f lastScale_;
    sf::Color lastColor_;
};
