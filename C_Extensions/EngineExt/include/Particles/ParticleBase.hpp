#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <functional>
#include <optional>

class ParticleSystem;

// BIND_CLASS
// Particle info structure containing position, colour, rotation and scale.
struct ParticleInfo {
    // BIND_PROPERTY
    sf::Vector2f position;

    // BIND_PROPERTY
    sf::Color color;

    // BIND_PROPERTY
    sf::Angle rotation;

    // BIND_PROPERTY
    sf::Vector2f scale;
};

// BIND_CLASS
// Base class for all particles.
class ParticleBase {
public:
    ParticleBase() = delete;

    // BIND_INIT
    ParticleBase(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction,
                 float countTime);

    // BIND_IGNORE
    void setParent(ParticleSystem* parent);

    // BIND_METHOD
    virtual void onTick(float deltaTime);

    // BIND_METHOD
    virtual void onLateTick(float deltaTime) {}

    // BIND_METHOD
    virtual void onFixedTick(float fixedDelta) {}

    // BIND_METHOD
    float getCountTime() const;

    // BIND_METHOD(return_policy="reference_internal")
    ParticleSystem* getParent() const;

protected:
    ParticleSystem* parent_;
    std::function<void(float, float, ParticleBase*)> moveFunction_;
    float countTime_;
};
