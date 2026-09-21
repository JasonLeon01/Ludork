#include "TransitionImpl.hpp"
#include <Manager/ShaderManager.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Manager/TextureManager.hpp>
#include <algorithm>
#include <utility>

namespace ludork::global::system_impl {

TransitionImpl::TransitionImpl(std::mutex& presentMutex)
    : presentMutex_(presentMutex) {}

float TransitionImpl::advanceElapsed(float elapsed, float duration,
                                     float deltaTime) {
    return std::min(elapsed + deltaTime, duration);
}

bool TransitionImpl::isComplete(float elapsed, float duration) {
    return elapsed >= duration;
}

void TransitionImpl::cacheTransitionBackground() {
    if (transition_ == nullptr || !transitionOutputSprite_.has_value()) {
        return;
    }
    transition_->clear(sf::Color::Transparent);
    transition_->draw(*transitionOutputSprite_, sf::BlendNone);
    transition_->display();
}

void TransitionImpl::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    ++transitionRevision_;
    transitionResource_ = transitionResource;
    if (transitionFreezePending_) {
        cacheTransitionBackground();
        transitionFreezePending_ = false;
        transitionFrozen_ = true;
    }
    if (transitionResource_ != nullptr && transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize = transitionResource_->getSize();
        const sf::Vector2u targetSize = transitionMaskTexture_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0 && targetSize.x > 0 &&
            targetSize.y > 0) {
            sf::Sprite maskSprite(*transitionResource_);
            maskSprite.setScale({static_cast<float>(targetSize.x) /
                                     static_cast<float>(sourceSize.x),
                                 static_cast<float>(targetSize.y) /
                                     static_cast<float>(sourceSize.y)});
            transitionMaskTexture_->clear(sf::Color::Transparent);
            transitionMaskTexture_->draw(maskSprite, sf::BlendNone);
            transitionMaskTexture_->display();
        }
    }
    if (!transitionFrozen_) {
        cacheTransitionBackground();
    } else {
        transitionFrozen_ = false;
    }
    if (transitionShader_ == nullptr) {
        inTransition_ = false;
        return;
    }
    inTransition_ = true;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = std::max(0.0f, transitionTime);
    if (transitionTempTexture_ != nullptr) {
        transitionTempTexture_->clear(sf::Color::Transparent);
    }
}

void TransitionImpl::freezeTransitionBackground() {
    transitionFreezePending_ = true;
}

bool TransitionImpl::isTransitionBackgroundFrozen() {
    return transitionFrozen_;
}

bool TransitionImpl::isTransitionBackgroundFreezePending() {
    return transitionFreezePending_;
}

void TransitionImpl::cancelTransitionBackgroundFreeze() {
    transitionFreezePending_ = false;
    transitionFrozen_ = false;
}

void TransitionImpl::requestTransition(
    std::optional<std::string> transitionName, float transitionTime) {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_ =
        PendingTransition{std::move(transitionName), transitionTime};
}

void TransitionImpl::cancelPendingTransition() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_.reset();
}

bool TransitionImpl::isTransitionPending() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    return pendingTransition_.has_value();
}

bool TransitionImpl::isInTransition() {
    return inTransition_;
}

void TransitionImpl::applyPendingTransition() {
    std::optional<PendingTransition> pending;
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pending.swap(pendingTransition_);
    }
    if (!pending.has_value()) {
        return;
    }
    std::shared_ptr<sf::Texture> resource;
    if (pending->name.has_value() && !pending->name->empty()) {
        resource = TextureManager::load(*pending->name);
    }
    setTransition(resource, pending->time);
}

void TransitionImpl::initializeTargets(const sf::Vector2u& size) {
    transition_ = std::make_unique<sf::RenderTexture>(size);
    transition_->clear(sf::Color::Transparent);
    transition_->display();
    transitionTempTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionTempTexture_->clear(sf::Color::Transparent);
    transitionOutputTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionOutputTexture_->clear(sf::Color::Transparent);
    transitionOutputTexture_->display();
    transitionMaskTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionMaskTexture_->clear(sf::Color::Transparent);
    transitionMaskTexture_->display();
    transitionSprite_.emplace(transitionTempTexture_->getTexture());
    transitionOutputSprite_.emplace(transitionOutputTexture_->getTexture());
}

void TransitionImpl::beginFrame() {
    transitionCompletionPending_ = false;
}

void TransitionImpl::advance(float deltaTime) {
    if (inTransition_) {
        transitionTimeCount_ =
            advanceElapsed(transitionTimeCount_, transitionTime_, deltaTime);
    }
}

void TransitionImpl::compose(const sf::Sprite& source,
                             sf::RenderTarget& target) {
    if (transitionOutputTexture_ == nullptr ||
        !transitionOutputSprite_.has_value()) {
        target.draw(source, canvasRenderStates());
    } else if (inTransition_ && transitionShader_ != nullptr &&
               transition_ != nullptr && transitionTempTexture_ != nullptr &&
               transitionSprite_.has_value()) {
        transitionTempTexture_->clear(sf::Color::Transparent);
        transitionTempTexture_->draw(source, sf::BlendNone);
        transitionTempTexture_->display();
        transitionShader_->setUniform("screenTex",
                                      transitionTempTexture_->getTexture());
        transitionShader_->setUniform("backTex", transition_->getTexture());
        transitionShader_->setUniform(
            "transitionResource",
            transitionResource_ != nullptr && transitionMaskTexture_ != nullptr
                ? transitionMaskTexture_->getTexture()
                : transition_->getTexture());
        transitionShader_->setUniform("useMask",
                                      transitionResource_ != nullptr &&
                                          transitionMaskTexture_ != nullptr);
        transitionShader_->setUniform("progress", transitionTimeCount_);
        transitionShader_->setUniform("totalTime", transitionTime_);
        sf::RenderStates states(sf::BlendNone);
        states.shader = transitionShader_.get();
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(*transitionSprite_, states);
        transitionOutputTexture_->display();
        target.draw(*transitionOutputSprite_, canvasRenderStates());
    } else {
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(source, sf::BlendNone);
        transitionOutputTexture_->display();
        target.draw(*transitionOutputSprite_, canvasRenderStates());
    }
}

void TransitionImpl::finishComposition() {
    if (transitionFreezePending_) {
        cacheTransitionBackground();
        transitionFreezePending_ = false;
        transitionFrozen_ = true;
    }
    composedTransitionRevision_ = transitionRevision_;
    transitionCompletionPending_ =
        inTransition_ && isComplete(transitionTimeCount_, transitionTime_);
}

void TransitionImpl::completeFrame(bool submitted) {
    if (submitted && transitionCompletionPending_ &&
        composedTransitionRevision_ == transitionRevision_) {
        inTransition_ = false;
    }
    transitionCompletionPending_ = false;
}

void TransitionImpl::initializeGraphics() {
    if (sf::Shader::isAvailable()) {
        transitionShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Transition.frag",
                                sf::Shader::Type::Fragment);
    } else {
        transitionShader_.reset();
        warnOnce("Transition.transitionShader",
                 "Shaders are unavailable; skipped loading transition shader");
    }
}

void TransitionImpl::rebuildTargets(const sf::Vector2u& size) {
    std::optional<sf::Image> transitionImage;
    if (transition_ != nullptr) {
        transition_->display();
        transitionImage = transition_->getTexture().copyToImage();
    }
    initializeTargets(size);
    if (transitionImage.has_value() && transition_ != nullptr) {
        const sf::Texture texture(*transitionImage);
        sf::Sprite sprite(texture);
        const sf::Vector2u sourceSize = texture.getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            transition_->clear(sf::Color::Transparent);
            transition_->draw(sprite, sf::BlendNone);
            transition_->display();
        }
    }
    if (transitionResource_ != nullptr && transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize = transitionResource_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sf::Sprite maskSprite(*transitionResource_);
            maskSprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            transitionMaskTexture_->clear(sf::Color::Transparent);
            transitionMaskTexture_->draw(maskSprite, sf::BlendNone);
            transitionMaskTexture_->display();
        }
    }
}

void TransitionImpl::reset() {
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pendingTransition_.reset();
    }
    transitionResource_.reset();
    transitionFrozen_ = false;
    transitionFreezePending_ = false;
    inTransition_ = false;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = 0.0f;
    transitionRevision_ = 0;
    composedTransitionRevision_ = 0;
    transitionCompletionPending_ = false;
    transitionShader_.reset();
}

void TransitionImpl::shutdown() noexcept {
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pendingTransition_.reset();
    }
    transitionResource_.reset();
    transitionShader_.reset();
    transitionSprite_.reset();
    transitionOutputSprite_.reset();
    transition_.reset();
    transitionTempTexture_.reset();
    transitionOutputTexture_.reset();
    transitionMaskTexture_.reset();
    inTransition_ = false;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = 0.0f;
    transitionRevision_ = 0;
    composedTransitionRevision_ = 0;
    transitionCompletionPending_ = false;
    transitionFrozen_ = false;
    transitionFreezePending_ = false;
}

}  // namespace ludork::global::system_impl
