#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

#include "Particles/ParticleBase.hpp"

class ParticleSystem;

class TextParticle : public ParticleBase, public sf::Text {
public:
    TextParticle() = delete;
    TextParticle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction, float countTime,
                 const std::string& text, const sf::Font& font, unsigned int characterSize);
};
