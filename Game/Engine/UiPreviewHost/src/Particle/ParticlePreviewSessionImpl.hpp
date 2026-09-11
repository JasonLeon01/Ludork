#pragma once

#include "Particle/ParticlePreviewSession.hpp"

#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Window/Context.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

class Emitter;

namespace ludork::preview_host {

struct ParticlePreviewSession::Impl {
    struct Session {
        Session();
        ~Session() noexcept;

        void activate();
        void configureTarget(const sf::Vector2u& size, float zoom);
        void rewind();
        bool seek(double time);
        RuntimeData frame(const std::string& sessionId, std::int64_t generation,
                          bool seeking, FrameFiles& frameFiles);

        sf::Context context;
        std::unique_ptr<sf::RenderTexture> target;
        std::unique_ptr<Emitter> emitter;
        int simulationRate = 60;
        std::optional<double> seekTime;
        std::int64_t seekSteps = 0;
        double timeOffset = 0;
    };

    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);

    std::unordered_map<std::string, std::unique_ptr<Session>> sessions;
    std::unordered_map<std::string, std::int64_t> generations;
};

}  // namespace ludork::preview_host
