#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

#include "Particles/ParticleBase.hpp"

class ParticleSystem;

// BIND_CLASS
// Text-based particle rendered as sf::Text.
class TextParticle : public ParticleBase, public sf::Text {
public:
    TextParticle() = delete;

    // BIND_INIT
    TextParticle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction, float countTime,
                 const std::string& text, const sf::Font& font, unsigned int characterSize);
};
