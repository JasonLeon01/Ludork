#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Particles/Particle.hpp"
#include "Particles/TextParticle.hpp"

class ParticleSystem;

class ParticleSystemSharedOwner
    : public std::enable_shared_from_this<ParticleSystem> {
protected:
    ParticleSystemSharedOwner() = default;
    ~ParticleSystemSharedOwner() = default;
};

////////////////////////////////////////////////////////////
/// \brief Manages particle lifetime, updates and rendering
///
////////////////////////////////////////////////////////////
BIND_CLASS(bind_bases = false, runtime_bases = "sf::Drawable",
           native_bases = "sf::Drawable", cast_bases = {"sf::Drawable"})
class LUDORK_ENGINE_API ParticleSystem : public sf::Drawable,
                                         public ParticleSystemSharedOwner {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) = delete;
    ParticleSystem& operator=(ParticleSystem&&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~ParticleSystem();

    ////////////////////////////////////////////////////////////
    /// \brief Add a sprite particle to the system
    ///
    /// - \param particle Particle instance to add
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void addParticle(const std::shared_ptr<Particle>& particle);

    ////////////////////////////////////////////////////////////
    /// \brief Add a text particle to the system
    ///
    /// - \param text Text particle instance to add
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void addText(const std::shared_ptr<TextParticle>& text);

    ////////////////////////////////////////////////////////////
    /// \brief Remove a sprite particle from the system
    ///
    /// - \param particle Particle instance to remove
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void removeParticle(Particle* particle);

    ////////////////////////////////////////////////////////////
    /// \brief Remove a text particle from the system
    ///
    /// - \param text Text particle instance to remove
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void removeText(TextParticle* text);

    ////////////////////////////////////////////////////////////
    /// \brief Remove particle by resource group and index
    ///
    /// - \param resourcePath Particle texture path key
    /// - \param index Index in grouped particle list
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void removeParticleAt(const std::string& resourcePath, int index);

    ////////////////////////////////////////////////////////////
    /// \brief Mark particle geometry as dirty
    ///
    /// - \param particle Dirty particle instance
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void addUpdateFlag(Particle* particle);

    ////////////////////////////////////////////////////////////
    /// \brief Apply pending particle geometry updates
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void updateParticlesInfo();

    ////////////////////////////////////////////////////////////
    /// \brief Execute per-frame updates for all particles
    ///
    /// - \param deltaTime Elapsed time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void onTick(float deltaTime);

    ////////////////////////////////////////////////////////////
    /// \brief Execute late updates for all particles
    ///
    /// - \param deltaTime Elapsed time in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void onLateTick(float deltaTime);

    ////////////////////////////////////////////////////////////
    /// \brief Execute fixed-step updates for all particles
    ///
    /// - \param fixedDelta Fixed time step in seconds
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void onFixedTick(float fixedDelta);

protected:
    ////////////////////////////////////////////////////////////
    /// \brief Draw all particle batches and text particles
    ///
    /// - \param target Destination render target
    /// - \param states Render states
    ///
    ////////////////////////////////////////////////////////////
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Build local quad points for a particle texture
    ///
    /// - \param particle Source particle
    ///
    /// - \return Quad corners in local space
    ///
    ////////////////////////////////////////////////////////////
    std::tuple<sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f>
    getUpdateParticleInfo(Particle* particle);
    std::unordered_map<std::string, std::vector<std::shared_ptr<Particle>>>
        particles_;
    std::unordered_map<std::string, sf::VertexArray> vertexArrays_;
    std::unordered_map<std::string, std::unique_ptr<sf::Texture>> resourceDict_;
    std::vector<std::shared_ptr<TextParticle>> texts_;
    std::unordered_map<std::string,
                       std::tuple<int, int, sf::Vector2f, sf::Vector2f,
                                  sf::Vector2f, sf::Vector2f>>
        textureUV_;
    std::vector<std::weak_ptr<Particle>> updateFlags_;
};
