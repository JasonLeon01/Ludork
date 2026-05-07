#pragma once

#include <BindAnnotations.hpp>

#include <SFML/Graphics.hpp>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Particles/Particle.hpp"
#include "Particles/TextParticle.hpp"


////////////////////////////////////////////////////////////
/// \brief Manages particle lifetime, updates and rendering
///
////////////////////////////////////////////////////////////
BIND_CLASS()
class ParticleSystem : public sf::Drawable {
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    BIND_INIT()
    ParticleSystem() {};

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
    void addParticle(Particle* particle);

    ////////////////////////////////////////////////////////////
    /// \brief Add a text particle to the system
    ///
    /// - \param text Text particle instance to add
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    void addText(TextParticle* text);

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
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Build local quad points for a particle texture
    ///
    /// - \param particle Source particle
    ///
    /// - \return Quad corners in local space
    ///
    ////////////////////////////////////////////////////////////
    std::tuple<sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f> getUpdateParticleInfo(Particle* particle);
    std::unordered_map<std::string, std::vector<Particle*>> particles_;
    std::unordered_map<std::string, sf::VertexArray> vertexArrays_;
    std::unordered_map<std::string, sf::Texture*> resourceDict_;
    std::vector<TextParticle*> texts_;
    std::unordered_map<std::string, std::tuple<int, int, sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f>>
        textureUV_;
    std::vector<Particle*> updateFlags_;
};
