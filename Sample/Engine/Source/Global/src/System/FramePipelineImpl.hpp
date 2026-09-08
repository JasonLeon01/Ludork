#pragma once

#include <System/GraphicsTypes.hpp>
#include <SFML/Graphics.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace ludork::global::system_impl {

class DisplayImpl;

class FramePipelineImpl {
public:
    void initCanvas(const sf::Vector2u& size);
    void draw(const sf::Drawable& drawable, sf::Shader* shader);
    void composeFrame(float deltaTime, sf::RenderTarget* target);
    void present(DisplayImpl& display);
    bool completeFrame(bool hasWindow);
    void addGraphicsShader(
        const std::shared_ptr<sf::Shader>& shader,
        std::optional<ShaderUniforms> uniforms = std::nullopt);
    void removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader);
    void removeAllGraphicsShaders();
    void removeGraphicsShaderAt(int index);
    void applyScreenTonePass();
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
    void setTransition(const std::shared_ptr<sf::Texture>& transitionResource,
                       float transitionTime);
    void freezeTransitionBackground();
    bool isTransitionBackgroundFrozen();
    bool isTransitionBackgroundFreezePending();
    void cancelTransitionBackgroundFreeze();
    void requestTransition(std::optional<std::string> transitionName,
                           float transitionTime);
    void cancelPendingTransition();
    bool isTransitionPending();
    bool isInTransition();
    void applyPendingTransition();
    void setWindowMapView(const sf::IntRect& rect);
    void setWindowDefaultView();
    sf::RenderTexture* getCanvas();
    void clearCanvas();
    sf::Vector2u getCanvasSize() const;
    void initializeGraphics();
    void rebuildTargets(const sf::Vector2u& size, float renderScale);
    void reset();
    void shutdown() noexcept;

private:
    void applyGraphicsShadersLength();
    void setShaderUniform(sf::Shader& shader, const std::string& name,
                          const ShaderUniformValue& value);
    bool shadersAvailable();
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
    float advanceElapsed(float elapsed, float duration, float deltaTime);
    bool isComplete(float elapsed, float duration);
    void cacheTransitionBackground();
    bool viewsEqual(const sf::View& left, const sf::View& right);
    struct PendingTransition {
        std::optional<std::string> name;
        float time = 1.0f;
    };
    bool canvasDefaultViewActive_ = true;
    std::unique_ptr<sf::RenderTexture> canvas_;
    std::optional<sf::Sprite> canvasSprite_;

    std::unique_ptr<sf::RenderTexture> transition_;
    std::unique_ptr<sf::RenderTexture> transitionTempTexture_;
    std::unique_ptr<sf::RenderTexture> transitionOutputTexture_;
    std::unique_ptr<sf::RenderTexture> transitionMaskTexture_;
    std::optional<sf::Sprite> transitionSprite_;
    std::optional<sf::Sprite> transitionOutputSprite_;
    std::vector<std::unique_ptr<sf::RenderTexture>> graphicsCanvases_;
    std::vector<std::shared_ptr<sf::Shader>> graphicsShaders_;
    std::shared_ptr<sf::Shader> transitionShader_;
    std::shared_ptr<sf::Texture> transitionResource_;
    bool inTransition_ = false;
    float transitionTimeCount_ = 0.0f;
    float transitionTime_ = 0.0f;
    std::size_t transitionRevision_ = 0;
    std::size_t composedTransitionRevision_ = 0;
    bool transitionCompletionPending_ = false;
    bool transitionFrozen_ = false;
    bool transitionFreezePending_ = false;
    std::optional<PendingTransition> pendingTransition_;
    std::mutex transitionMutex_;
    std::mutex presentMutex_;

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
