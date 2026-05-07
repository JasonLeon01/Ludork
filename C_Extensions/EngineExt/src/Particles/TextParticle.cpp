#include "Particles/TextParticle.hpp"

#include "Particles/ParticleSystem.hpp"

sf::String toSFString(const std::string& str) {
    return sf::String::fromUtf8(str.begin(), str.end());
}

std::string toUTF8String(const sf::String& str) {
    auto utf8Bytes = str.toUtf8();
    return std::string(utf8Bytes.begin(), utf8Bytes.end());
}

TextParticle::TextParticle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction,
                           float countTime, const std::string& text, const sf::Font& font, unsigned int characterSize)
    : ParticleBase(parent, moveFunction, countTime),
      sf::Text(font, toSFString(text), characterSize) {}
