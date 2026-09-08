#include "FramePipelineImpl.hpp"
#include <Manager/TextureManager.hpp>
#include <algorithm>
#include <utility>

namespace ludork::global::system_impl {

float FramePipelineImpl::advanceElapsed(float elapsed, float duration,
                                        float deltaTime) {
    return std::min(elapsed + deltaTime, duration);
}

bool FramePipelineImpl::isComplete(float elapsed, float duration) {
    return elapsed >= duration;
}

void FramePipelineImpl::cacheTransitionBackground() {
    if (transition_ == nullptr || !transitionOutputSprite_.has_value()) {
        return;
    }
    transition_->clear(sf::Color::Transparent);
    transition_->draw(*transitionOutputSprite_, sf::BlendNone);
    transition_->display();
}

void FramePipelineImpl::setTransition(
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

void FramePipelineImpl::freezeTransitionBackground() {
    transitionFreezePending_ = true;
}

bool FramePipelineImpl::isTransitionBackgroundFrozen() {
    return transitionFrozen_;
}

bool FramePipelineImpl::isTransitionBackgroundFreezePending() {
    return transitionFreezePending_;
}

void FramePipelineImpl::cancelTransitionBackgroundFreeze() {
    transitionFreezePending_ = false;
    transitionFrozen_ = false;
}

void FramePipelineImpl::requestTransition(
    std::optional<std::string> transitionName, float transitionTime) {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_ =
        PendingTransition{std::move(transitionName), transitionTime};
}

void FramePipelineImpl::cancelPendingTransition() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_.reset();
}

bool FramePipelineImpl::isTransitionPending() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    return pendingTransition_.has_value();
}

bool FramePipelineImpl::isInTransition() {
    return inTransition_;
}

void FramePipelineImpl::applyPendingTransition() {
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

}  // namespace ludork::global::system_impl
