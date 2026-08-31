#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <functional>
#include <memory>
#include <optional>

class LUDORK_ENGINE_API ParticleSystem;

////////////////////////////////////////////////////////////
/// \brief Runtime particle transform and color state
///
////////////////////////////////////////////////////////////
BIND_CLASS()
struct ParticleInfo {
    BIND_INIT()
    ParticleInfo() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Particle position in world space
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    sf::Vector2f position;

    ////////////////////////////////////////////////////////////
    /// \brief Particle tint color
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    sf::Color color;

    ////////////////////////////////////////////////////////////
    /// \brief Particle rotation angle
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    sf::Angle rotation;

    ////////////////////////////////////////////////////////////
    /// \brief Particle scale factor
    ///
    ////////////////////////////////////////////////////////////
    BIND_PROPERTY()
    sf::Vector2f scale;
};

////////////////////////////////////////////////////////////
/// \brief Base class for all particle entities
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class LUDORK_ENGINE_API ParticleBase {
public:
    ParticleBase() = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Construct a particle base instance
    ///
    /// - \param parent Owning particle system
    /// - \param moveFunction Callback executed on tick update
    /// - \param countTime Initial accumulated time
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    ParticleBase(std::shared_ptr<ParticleSystem> parent,
                 std::function<void(float, float, ParticleBase*)> moveFunction,
                 float countTime);

    ////////////////////////////////////////////////////////////
    /// \brief Set the current parent particle system
    ///
    /// - \param parent New owner or `nullptr`
    ///
    ////////////////////////////////////////////////////////////
    void setParent(std::shared_ptr<ParticleSystem> parent);

    ////////////////////////////////////////////////////////////
    /// \brief Execute per-frame particle update
    ///
    /// - \param deltaTime Elapsed time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void onTick(float deltaTime);

    ////////////////////////////////////////////////////////////
    /// \brief Execute late update phase
    ///
    /// - \param deltaTime Elapsed time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void onLateTick(float deltaTime) {}

    ////////////////////////////////////////////////////////////
    /// \brief Execute fixed-step update phase
    ///
    /// - \param fixedDelta Fixed time step in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    virtual void onFixedTick(float fixedDelta) {}

    ////////////////////////////////////////////////////////////
    /// \brief Get current accumulated lifetime
    ///
    /// - \return Accumulated time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    float getCountTime() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get current owning particle system
    ///
    /// - \return Owning particle system pointer
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    std::shared_ptr<ParticleSystem> getParent() const;

protected:
    std::weak_ptr<ParticleSystem> parent_;
    std::function<void(float, float, ParticleBase*)> moveFunction_;
    float countTime_;
};
