#pragma once
#include <Runtime/Graphics/GpuEmitterConfiguration.hpp>
#include "GpuResourcesImpl.hpp"
#include <SFML/Graphics/Texture.hpp>
#include <array>
#include <memory>

namespace ludork::runtime::graphics {
class GpuTrackImpl {
public:
    GpuEmitterConfiguration::Track definition;
    std::shared_ptr<GpuResourcesImpl> resources;
    std::shared_ptr<sf::Texture> texture;
    unsigned int buffers[2]{};
    unsigned int curves = 0;
    int input = 0;
    std::uint64_t births = 0;
    double credit = 0;
    int pending = 0;
    bool initializedResident = false;
    float lastBirth = 0;
    float warmTime = 0;
    float warmEnd = 0;
    GpuTrackImpl(const GpuEmitterConfiguration::Track& data,
                 std::shared_ptr<GpuResourcesImpl> gpu);
    ~GpuTrackImpl();
    void reset();
    void step(float previous, float current, float delta, float distance,
              const sf::Vector2f& motion, const sf::Transform& host, int seed,
              bool draining);
    void draw(sf::RenderTarget& target, const sf::RenderStates& states,
              const sf::Transform& host, const sf::Color& colour);

private:
    std::uint32_t seedSalt_ = 2166136261u;
    void bindState(bool instanced);
    void simulate(float delta, int count, const sf::Vector2f& motion,
                  const sf::Transform& host, int seed, bool draining,
                  bool reset);
    sf::Transform transform(const sf::Transform& host,
                            sf::Vector2f& shapeScale) const;
};
}  // namespace ludork::runtime::graphics
