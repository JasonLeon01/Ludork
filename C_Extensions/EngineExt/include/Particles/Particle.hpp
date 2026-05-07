#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics.hpp>
#include <string>

#include "Particles/ParticleBase.hpp"

////////////////////////////////////////////////////////////
/// \brief Sprite-based particle entry managed by `ParticleSystem`
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class Particle : public ParticleBase {
public:
    Particle() = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Construct a sprite particle
    ///
    /// - \param parent Owning particle system
    /// - \param moveFunction Callback executed on each update
    /// - \param countTime Initial accumulated time
    /// - \param inResourcePath Texture resource path
    /// - \param inInfo Initial particle transform/color state
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    Particle(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction, float countTime,
             const std::string& inResourcePath, ParticleInfo inInfo);

    ////////////////////////////////////////////////////////////
    /// \brief Execute per-frame update logic
    ///
    /// - \param deltaTime Elapsed time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void onTick(float deltaTime) override;

    ////////////////////////////////////////////////////////////
    /// \brief Execute late update logic
    ///
    /// - \param deltaTime Elapsed time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void onLateTick(float deltaTime) override;

    ////////////////////////////////////////////////////////////
    /// \brief Execute fixed-step update logic
    ///
    /// - \param fixedDelta Fixed time step in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void onFixedTick(float fixedDelta) override;

    ////////////////////////////////////////////////////////////
    /// \brief Remove this particle from its parent system
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void destroy();

    ////////////////////////////////////////////////////////////
    /// \brief Texture resource path used by this particle
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    std::string resourcePath;

    ////////////////////////////////////////////////////////////
    /// \brief Mutable particle transform and color state
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    ParticleInfo info;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Track transform/color changes and schedule geometry update
    ///
    ////////////////////////////////////////////////////////////
    void checkUpdate();
    sf::Vector2f lastPosition_;
    sf::Angle lastRotation_;
    sf::Vector2f lastScale_;
    sf::Color lastColor_;
};
