#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Particles/Particle.hpp"
#include "Particles/TextParticle.hpp"


// BIND_CLASS
// Manages all particles and handles rendering via vertex arrays.
class ParticleSystem : public sf::Drawable {
public:
    // BIND_INIT
    ParticleSystem() {};
    ~ParticleSystem();

    // BIND_METHOD
    void addParticle(Particle* particle);

    // BIND_METHOD
    void addText(TextParticle* text);

    // BIND_METHOD
    void removeParticle(Particle* particle);

    // BIND_METHOD
    void removeText(TextParticle* text);

    // BIND_METHOD
    void removeParticleAt(const std::string& resourcePath, int index);

    // BIND_METHOD
    void addUpdateFlag(Particle* particle);

    // BIND_METHOD
    void updateParticlesInfo();

    // BIND_METHOD
    void onTick(float deltaTime);

    // BIND_METHOD
    void onLateTick(float deltaTime);

    // BIND_METHOD
    void onFixedTick(float fixedDelta);

protected:
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    std::tuple<sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f> getUpdateParticleInfo(Particle* particle);
    std::unordered_map<std::string, std::vector<Particle*>> particles_;
    std::unordered_map<std::string, sf::VertexArray> vertexArrays_;
    std::unordered_map<std::string, sf::Texture*> resourceDict_;
    std::vector<TextParticle*> texts_;
    std::unordered_map<std::string, std::tuple<int, int, sf::Vector2f, sf::Vector2f, sf::Vector2f, sf::Vector2f>>
        textureUV_;
    std::vector<Particle*> updateFlags_;
};
