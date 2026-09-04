#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <Curve.hpp>
#include <Particles/ParticleSystem.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TextParticle;
struct PlainTextConfig;

BIND_CLASS()
class CommonTipController {
public:
    BIND_INIT()
    explicit CommonTipController(
        std::shared_ptr<ParticleSystem> particleSystem);

    BIND_METHOD()
    void addTip(const std::string& text);

    BIND_METHOD()
    void onTick(float deltaTime);

    static void shutdown() noexcept;

private:
    enum class TipPhase {
        FadeIn,
        Queued,
        Wait,
        FadeOut
    };

    struct TipItem {
        std::shared_ptr<TextParticle> textParticle;
        float screenY = 0.0f;
        float targetScreenY = 0.0f;
        float alpha = 0.0f;
        TipPhase phase = TipPhase::FadeIn;
        float timer = 0.0f;
        float fadeProgress = 0.0f;
    };

    void removeTopTip();
    void syncDisplayScale();
    void updatePlacement();
    float evaluateFadeInAlpha(float elapsed) const;
    float evaluateFadeOutAlpha(float elapsed) const;
    float evaluateFadeOutRise(float elapsed) const;
    float getFadeInDuration() const;
    float getFadeOutDuration() const;
    static std::shared_ptr<Curve> getCurve(const std::string& key);
    static std::shared_ptr<PlainTextConfig> getTextConfig();
    static float getScaledDistance(float logicalValue);
    static float getScaledScreenY(std::size_t index);

    std::shared_ptr<ParticleSystem> particleSystem_;
    std::vector<TipItem> tips_;
    bool shifting_ = false;
    float displayScale_ = 1.0f;

    static std::unordered_map<std::string, std::shared_ptr<Curve>> curves_;
    static std::shared_ptr<PlainTextConfig> textConfig_;
};
