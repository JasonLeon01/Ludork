#pragma once

#include <SFML/Graphics.hpp>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace ludork::global::system_impl {

class TransitionImpl {
public:
    explicit TransitionImpl(std::mutex& presentMutex);
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
    void initializeTargets(const sf::Vector2u& size);
    void beginFrame();
    void advance(float deltaTime);
    void compose(const sf::Sprite& source, sf::RenderTarget& target);
    void finishComposition();
    void completeFrame(bool submitted);
    void initializeGraphics();
    void rebuildTargets(const sf::Vector2u& size);
    void reset();
    void shutdown() noexcept;

private:
    float advanceElapsed(float elapsed, float duration, float deltaTime);
    bool isComplete(float elapsed, float duration);
    void cacheTransitionBackground();
    struct PendingTransition {
        std::optional<std::string> name;
        float time = 1.0f;
    };
    std::unique_ptr<sf::RenderTexture> transition_;
    std::unique_ptr<sf::RenderTexture> transitionTempTexture_;
    std::unique_ptr<sf::RenderTexture> transitionOutputTexture_;
    std::unique_ptr<sf::RenderTexture> transitionMaskTexture_;
    std::optional<sf::Sprite> transitionSprite_;
    std::optional<sf::Sprite> transitionOutputSprite_;
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
    std::mutex& presentMutex_;
};

TransitionImpl& transitionImpl();

}  // namespace ludork::global::system_impl
