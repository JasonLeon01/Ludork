#include "TransitionImpl.hpp"
#include "SystemImpl.hpp"
#include <Manager/TextureManager.hpp>
#include <algorithm>
#include <utility>

namespace ludork::global::system_transition_impl {

float advanceElapsed(float elapsed, float duration, float deltaTime) {
    return std::min(elapsed + deltaTime, duration);
}

bool isComplete(float elapsed, float duration) {
    return elapsed >= duration;
}

void cacheTransitionBackground() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    if (framePipeline.transition_ == nullptr ||
        !framePipeline.transitionOutputSprite_.has_value()) {
        return;
    }
    framePipeline.transition_->clear(sf::Color::Transparent);
    framePipeline.transition_->draw(*framePipeline.transitionOutputSprite_,
                                    sf::BlendNone);
    framePipeline.transition_->display();
}

void setTransition(const std::shared_ptr<sf::Texture>& transitionResource,
                   float transitionTime) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.presentMutex_);
    ++framePipeline.transitionRevision_;
    framePipeline.transitionResource_ = transitionResource;
    if (framePipeline.transitionFreezePending_) {
        ludork::global::system_transition_impl::cacheTransitionBackground();
        framePipeline.transitionFreezePending_ = false;
        framePipeline.transitionFrozen_ = true;
    }
    if (framePipeline.transitionResource_ != nullptr &&
        framePipeline.transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize =
            framePipeline.transitionResource_->getSize();
        const sf::Vector2u targetSize =
            framePipeline.transitionMaskTexture_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0 && targetSize.x > 0 &&
            targetSize.y > 0) {
            sf::Sprite maskSprite(*framePipeline.transitionResource_);
            maskSprite.setScale({static_cast<float>(targetSize.x) /
                                     static_cast<float>(sourceSize.x),
                                 static_cast<float>(targetSize.y) /
                                     static_cast<float>(sourceSize.y)});
            framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
            framePipeline.transitionMaskTexture_->draw(maskSprite,
                                                       sf::BlendNone);
            framePipeline.transitionMaskTexture_->display();
        }
    }
    if (!framePipeline.transitionFrozen_) {
        ludork::global::system_transition_impl::cacheTransitionBackground();
    } else {
        framePipeline.transitionFrozen_ = false;
    }
    if (framePipeline.transitionShader_ == nullptr) {
        framePipeline.inTransition_ = false;
        return;
    }
    framePipeline.inTransition_ = true;
    framePipeline.transitionTimeCount_ = 0.0f;
    framePipeline.transitionTime_ = std::max(0.0f, transitionTime);
    if (framePipeline.transitionTempTexture_ != nullptr) {
        framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
    }
}

void freezeTransitionBackground() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.transitionFreezePending_ = true;
}

bool isTransitionBackgroundFrozen() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return framePipeline.transitionFrozen_;
}

bool isTransitionBackgroundFreezePending() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return framePipeline.transitionFreezePending_;
}

void cancelTransitionBackgroundFreeze() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    framePipeline.transitionFreezePending_ = false;
    framePipeline.transitionFrozen_ = false;
}

void requestTransition(std::optional<std::string> transitionName,
                       float transitionTime) {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    framePipeline.pendingTransition_ =
        ludork::global::system_impl::PendingTransition{
            std::move(transitionName), transitionTime};
}

void cancelPendingTransition() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    framePipeline.pendingTransition_.reset();
}

bool isTransitionPending() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    return framePipeline.pendingTransition_.has_value();
}

bool isInTransition() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    return framePipeline.inTransition_;
}

void applyPendingTransition() {
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        ludork::global::system_impl::impl().framePipeline;
    std::optional<ludork::global::system_impl::PendingTransition> pending;
    {
        const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
        pending.swap(framePipeline.pendingTransition_);
    }
    if (!pending.has_value()) {
        return;
    }
    std::shared_ptr<sf::Texture> resource;
    if (pending->name.has_value() && !pending->name->empty()) {
        resource = TextureManager::load(*pending->name);
    }
    ludork::global::system_transition_impl::setTransition(resource,
                                                          pending->time);
}

}  // namespace ludork::global::system_transition_impl
