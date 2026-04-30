#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Particles/Particle.hpp"
#include "Particles/TextParticle.hpp"


class ParticleSystem : public sf::Drawable {
public:
    ParticleSystem() {};
    ~ParticleSystem();
    void addParticle(Particle* particle);
    void addText(TextParticle* text);
    void removeParticle(Particle* particle);
    void removeText(TextParticle* text);
    void removeParticleAt(const std::string& resourcePath, int index);
    void addUpdateFlag(Particle* particle);
    void updateParticlesInfo();
    void onTick(float deltaTime);
    void onLateTick(float deltaTime);
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
