#include "Particles/TextParticle.hpp"

#include "Particles/ParticleSystem.hpp"

TextParticle::TextParticle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction,
                           float countTime, const std::string& text, const sf::Font& font, unsigned int characterSize)
    : ParticleBase(parent, moveFunction, countTime), sf::Text(font, text, characterSize) {}
