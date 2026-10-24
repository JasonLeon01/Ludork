#pragma once

#include <Runtime/RuntimeData.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sf {
class Context;
class RenderTexture;
}  // namespace sf

class UiAssetInstance;
class EmitterScheduler;
class EmitterView;

namespace ludork::preview_host {

class FrameFiles;

class UiPreviewSession {
public:
    UiPreviewSession();
    ~UiPreviewSession();

    struct RenderTargetSpec {
        float renderScale;
        sf::Vector2u size;
    };

    void reset() noexcept;
    RuntimeData render(const RuntimeData::Map& request, FrameFiles& frameFiles);
    RuntimeData hitTest(const RuntimeData::Map& request) const;
    RuntimeData resolveReparent(const RuntimeData::Map& request);

private:
    static constexpr char ReparentResponse[] = "reparentSlot";
    static constexpr char HitTestResponse[] = "hitTest";
    static constexpr char FrameResponse[] = "frame";
    static constexpr int ParticleSimulationRate = 240;
    static constexpr int MaximumParticleSeekSteps = 120;
    static constexpr double ParticleSeekBudgetMilliseconds = 12.0;
    static constexpr double ParticleStepRoundingTolerance = 0.000001;
    static constexpr float DefaultAnimationSampleTime = 0.5f;

    void resetContent() noexcept;
    bool sampleParticles(double time);

    std::unique_ptr<sf::Context> context_;
    std::unique_ptr<sf::RenderTexture> target_;
    std::unique_ptr<EmitterScheduler> emitterScheduler_;
    std::shared_ptr<UiAssetInstance> instance_;
    std::vector<std::shared_ptr<EmitterView>> emitterViews_;
    std::string snapshot_;
    std::string particleResources_;
    std::string animationName_;
    std::optional<std::string> animationTarget_;
    std::int64_t particleSteps_ = 0;
    std::int64_t generation_ = 0;
    sf::Vector2u designSize_;
    sf::Vector2u renderSize_;
    float renderScale_ = 1.0f;
};

}  // namespace ludork::preview_host
