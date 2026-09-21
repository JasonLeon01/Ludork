#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <random>

namespace ludork::global::system_impl {

class GraphicsShaderSink;

class ScreenEffectsImpl {
public:
    explicit ScreenEffectsImpl(GraphicsShaderSink& shaders);
    void applyScreenTonePass(sf::RenderTexture* canvas);
    void flashScreen(std::optional<sf::Color> color, float duration);
    void stopFlash();
    bool isFlashing();
    void changeScreenTone(float red, float green, float blue, float gray,
                          float duration);
    void clearScreenTone(float duration);
    void stopScreenTone();
    bool isScreenToneActive();
    bool isScreenToneTransitionComplete();
    void startShake(float power, float speed, float duration);
    void stopShake();
    bool isShaking();
    void update(float deltaTime);
    void applyShake(sf::Sprite& sprite, const sf::Vector2u& textureSize);
    void restoreShake(sf::Sprite& sprite);
    void invalidateTargets();
    void reset();
    void shutdown() noexcept;

private:
    sf::Glsl::Vec4 makeToneColour(float red, float green, float blue,
                                  float gray);
    sf::Glsl::Vec4 interpolateTone(const sf::Glsl::Vec4& start,
                                   const sf::Glsl::Vec4& target, float ratio);
    bool isNeutralTone(const sf::Glsl::Vec4& colour);
    void updateFlash(float deltaTime);
    void updateScreenTone(float deltaTime);
    void updateShake(float deltaTime);
    bool ensureToneShader();
    void applyScreenToneUniform();
    void ensureToneBuffer(const sf::Vector2u& size);
    GraphicsShaderSink& shaders_;
    std::shared_ptr<sf::Shader> flashShader_;
    sf::Glsl::Vec4 flashColour_{1.0f, 1.0f, 1.0f, 1.0f};
    float flashDuration_ = 0.0f;
    float flashTimeCount_ = 0.0f;
    bool flashActive_ = false;

    std::shared_ptr<sf::Shader> toneShader_;
    sf::Glsl::Vec4 toneCurrentColour_{};
    sf::Glsl::Vec4 toneStartColour_{};
    sf::Glsl::Vec4 toneTargetColour_{};
    float toneDuration_ = 0.0f;
    float toneTimeCount_ = 0.0f;
    bool toneActive_ = false;
    std::unique_ptr<sf::RenderTexture> toneBuffer_;
    std::optional<sf::Sprite> toneBufferSprite_;

    float shakePower_ = 0.0f;
    float shakeSpeed_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimeCount_ = 0.0f;
    bool shakeActive_ = false;
    sf::Vector2f shakeOffset_{};
    float shakeNextUpdate_ = 0.0f;
    std::mt19937 random_{std::random_device{}()};
};

}  // namespace ludork::global::system_impl
