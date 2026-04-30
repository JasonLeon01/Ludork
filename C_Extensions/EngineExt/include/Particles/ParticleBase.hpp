#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <functional>
#include <optional>

class ParticleSystem;

struct ParticleInfo {
    sf::Vector2f position;
    sf::Color color;
    sf::Angle rotation;
    sf::Vector2f scale;
};

class ParticleBase {
public:
    ParticleBase() = delete;
    ParticleBase(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction,
                 float countTime);
    void setParent(ParticleSystem* parent);
    virtual void onTick(float deltaTime);
    virtual void onLateTick(float deltaTime) {}
    virtual void onFixedTick(float fixedDelta) {}
    float getCountTime() const;
    ParticleSystem* getParent() const;

protected:
    ParticleSystem* parent_;
    std::function<void(float, float, ParticleBase*)> moveFunction_;
    float countTime_;
};
