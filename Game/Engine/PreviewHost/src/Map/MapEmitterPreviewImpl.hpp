#pragma once

#include <Emitters/Emitter.hpp>
#include <Graphics/MapRenderer.hpp>
#include <Runtime/RuntimeData.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace ludork::preview_host {

class MapEmitterPreviewImpl {
public:
    void update(const RuntimeData::Map& data, const MapRenderer::Scene& scene);
    void advance(float delta, bool resetMotion);
    MapRenderer::DrawHooks drawHooks();
    void clear(bool releaseResources = false) noexcept;

private:
    struct Entry {
        std::unique_ptr<Emitter> emitter;
        std::string resource;
        std::string signature;
        sf::Transform transform;
        sf::Color colour = sf::Color::White;
        bool beforeActor = false;
        bool initialize = true;
    };

    struct Resource {
        std::string signature;
        EmitterConfiguration configuration;
    };

    void draw(std::uint64_t id, sf::RenderTarget& target,
              const sf::RenderStates& states, bool before);

    std::unordered_map<std::uint64_t, Entry> entries_;
    std::unordered_map<std::string, Resource> resources_;
    std::unique_ptr<sf::RenderTexture> simulationTarget_;
};

}  // namespace ludork::preview_host
