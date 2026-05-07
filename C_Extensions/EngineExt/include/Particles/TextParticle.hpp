#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

#include "Particles/ParticleBase.hpp"

class ParticleSystem;

////////////////////////////////////////////////////////////
/// \brief Text-based particle rendered through `sf::Text`
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class TextParticle : public ParticleBase, public sf::Text {
public:
    TextParticle() = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Construct a text particle
    ///
    /// - \param parent Owning particle system
    /// - \param moveFunction Callback executed on each update
    /// - \param countTime Initial accumulated time
    /// - \param text Initial text content
    /// - \param font Font used for rendering
    /// - \param characterSize Character size in pixels
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    TextParticle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction, float countTime,
                 const std::string& text, const sf::Font& font, unsigned int characterSize);
};
